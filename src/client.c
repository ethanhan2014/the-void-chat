#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "util.h"
#include "client.h"

void client_start(machine_info* mach) {
  int s = open_socket(mach);
  messageQueue = (linkedList *) malloc(sizeof(linkedList));
  messageQueue->length = 0;
  tempBuff = (linkedList *) malloc(sizeof(linkedList));
  tempBuff->length = 0;
  printf("%s joining a new chat on %s:%d, listening on %s:%d\n", mach->name, 
    mach->host_ip, mach->host_port, mach->ipaddr, mach->portno);
  message host_attempt = join_request(mach);

  //real host info is in contents of message if we did not contact leader
  if (!host_attempt.header.about.isLeader) {
    char ip_port[BUFSIZE];
    strcpy(ip_port, host_attempt.content);
    char* new_ip = strtok(ip_port, ":");
    int new_port = strtol(strtok(NULL, " "), 0, 10);

    strcpy(mach->host_ip, new_ip);
    mach->host_port = new_port;

    host_attempt = join_request(mach);
  }
  latestSequenceNum = host_attempt.header.about.current_sequence_num - 1;
  update_clients(mach, host_attempt.header.about);

  printf("Succeeded, current users:\n");
  print_users(mach);
  printf("Current sequence number received: %d\n", latestSequenceNum);
  //we have leader info inside of host_attempt message now
  client_loop(mach, s);

  close(s);
}

void client_loop(machine_info* mach, int s) {
  //kick off a thread that is listening in parallel
  thread_params params;
  params.mach = mach;
  params.socket = s;

  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, client_listen, &params)) {
    perror("Error making thread to parse incoming message");
    exit(1);
  }

  pthread_t printer_thread;
  if (pthread_create(&printer_thread, NULL, sortAndPrint, NULL)) {
    perror("Oh no! We don't have a printer thread! We're screwed!");
    exit(1);
  }

  //loop waiting for user input
  int done = FALSE;
  while (!done) {
    char input[BUFSIZE]; //get user input (messages)
    if (scanf("%s", input) == EOF) {
      //on ctrl-d (EOF), kill this program instead of interpreting input
      done = TRUE;
      quit_notice(mach);
    } else {
      msg_request(mach, input); //do nothing with response right now
    }
  }
}

message join_request(machine_info* mach) {
  int s; //the socket
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error making socket to send message");
    exit(1);
  }
  //give socket a timeout
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("Error setting socket timeout parameter");
    exit(1);
  }

  //make message to send
  message init_conn;
  msg_header header;
  header.timestamp = (int)time(0);
  header.msg_type = JOIN_REQ;
  header.status = TRUE;
  header.about = *mach;
  init_conn.header = header;

  //setup host info to connect to/send message to
  char ip_copy[BUFSIZE]; //inet_addr changes ip(?)... use a copy of it to avoid
  strcpy(ip_copy, mach->host_ip); 
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip_copy);
  server.sin_port = htons(mach->host_port);

  if (sendto(s, &init_conn, sizeof(message), 0, (struct sockaddr *)&server, 
      (socklen_t)sizeof(struct sockaddr)) < 0) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  message response;
  if (receive_message(s, &response, NULL, mach) == FALSE) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  close(s);

  return response;
}

message msg_request(machine_info* mach, char msg[BUFSIZE]) {
  int s; //the socket
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error making socket to send message");
    exit(1);
  }
  //give socket a timeout
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("Error setting socket timeout parameter");
  }

  //make message to send
  message text_msg;
  msg_header header;
  header.timestamp = (int)time(0);
  header.msg_type = MSG_REQ;
  header.status = TRUE;
  header.about = *mach;
  text_msg.header = header;
  strcpy(text_msg.content, msg);

  //setup host info to connect to/send message to
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(mach->host_ip);
  server.sin_port = htons(mach->host_port);

  if (sendto(s, &text_msg, sizeof(message), 0, (struct sockaddr *)&server, 
      (socklen_t)sizeof(struct sockaddr)) < 0) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  message response;
  if (receive_message(s, &response, NULL, mach) == FALSE) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  close(s);

  return response;
}

void quit_notice(machine_info* mach) {
  int s; //the socket
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error making socket to send message");
    exit(1);
  }
  //give socket a timeout
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("Error setting socket timeout parameter");
  }

  //make message to send
  message quit_msg;
  msg_header header;
  header.timestamp = (int)time(0);
  header.msg_type = QUIT;
  header.status = TRUE;
  header.about = *mach;
  quit_msg.header = header;

  //setup host info to connect to/send message to
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(mach->host_ip);
  server.sin_port = htons(mach->host_port);

  if (sendto(s, &quit_msg, sizeof(message), 0, (struct sockaddr *)&server, 
      (socklen_t)sizeof(struct sockaddr)) < 0) {
    printf("Note: could not send quit message to leader, quitting anyway\n");
  }

  close(s);
}

void parse_incoming_cl(message m, machine_info* mach, struct sockaddr_in source,
    int s) {
  if (m.header.msg_type == JOIN_REQ) {
    //pack up a lovely response message
    message respond_join;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = JOIN_RES;
    header.status = TRUE;
    header.about = *mach;
    respond_join.header = header;

    sprintf(respond_join.content, "%s:%d", mach->host_ip, mach->host_port);

    if (sendto(s, &respond_join , sizeof(respond_join), 0, 
        (struct sockaddr*)&source, sizeof(source)) < 0) {
      perror("Error responding to client with host info");
      exit(1);
    }
  } else {
    //well we don't want to print just yet now do we cutie pie?
    //print_message(m);
    //let's add this to a temp buff and sort it somewhere else
    addElement(tempBuff, m.header.seq_num, "", m);
  }
}

int receive_message(int s, message* m, struct sockaddr_in* source, 
    machine_info* mach) {
  socklen_t sourcelen = sizeof(*source);
  if (recvfrom(s, m, sizeof(message), 0, (struct sockaddr*)source, 
      &sourcelen) < 0) {
    return FALSE;
  }

  if (m->header.about.isLeader) {
    update_clients(mach, m->header.about);
  }

  return TRUE;
}

void* client_listen(void* input) {
  thread_params* params = (thread_params*)input;

  int done = FALSE;
  while (!done) {
    message incoming;
    struct sockaddr_in source;

    //wait for a message + update clients every time if message from leader
    if (receive_message(params->socket, &incoming, &source, params->mach) == FALSE) {
      done = TRUE; //socket was closed somewhere/became invalid; kill thread
    } else {
      parse_incoming_cl(incoming, params->mach, source, params->socket); //deal with the message
    }
  }

  pthread_exit(0);
}

void* sortAndPrint() {
  int done = FALSE;

  while (!done) {
    //we simply find the next one in the sequence if we can
    //otherwise, we hold
    int i = 0;
    int found = FALSE;
    for (i = 0; i < tempBuff->length; i++) {
      if (getElement(tempBuff, i)->v == latestSequenceNum + 1) {
        found = TRUE;
        break;
      }
    }
    //we found the message
    //rejoice!
    if (found) {
      print_message(getElement(tempBuff, i)->m);
      latestSequenceNum++;
    }
    else { //well, we didn't find it, so we gotta wait a little it looks like, or something

    }
  }
  pthread_exit(0);
}