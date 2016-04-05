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

void client_start(char* ip, int port, machine_info* mach) {
  int s = open_socket(mach);

  printf("%s joining a new chat on %s:%d, listening on %s:%d\n", mach->name, 
    ip, port, mach->ipaddr, mach->portno);
  char ip_copy[BUFSIZE]; strcpy(ip_copy, ip); //join_request changes ip
  message host_attempt = join_request(ip_copy, port, mach);

  //real host info is in contents of message if we did not contact leader
  if (!host_attempt.header.about.isLeader) {
    char ip_port[BUFSIZE];
    strcpy(ip_port, host_attempt.content);
    ip = strtok(ip_port, ":");
    port = strtol(strtok(NULL, " "), 0, 10);

    host_attempt = join_request(ip, port, mach);
  }
  update_clients(mach, host_attempt.header.about);

  printf("Succeeded, current users:\n");
  print_users(mach);

  //we have leader info inside of host_attempt message now
  client_loop(ip, port, mach, s);

  close(s);
}

void client_loop(char* ip, int port, machine_info* mach, int s) {
  //kick off a thread that is listening in parallel
  thread_params params;
  params.mach = mach;
  strcpy(params.host_ip, ip);
  params.host_port = port;
  params.socket = s;

  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, client_listen, &params)) {
    perror("Error making thread to parse incoming message");
    exit(1);
  }

  //loop waiting for user input
  int done = FALSE;
  while (!done) {
    char input[BUFSIZE];
    scanf("%s", input);  //get user input (messages)
    msg_request(ip, port, mach, input); //do nothing with response right now
  }
}

message join_request(char* ip, int port, machine_info* mach) {
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
    perror("Error setting socket timeout parameter\n");
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
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip);
  server.sin_port = htons(port);

  if (sendto(s, &init_conn, sizeof(message), 0, (struct sockaddr *)&server, 
      (socklen_t)sizeof(struct sockaddr)) < 0) {
    perror("No chat active at this address, try again later\n");
    exit(1);
  }

  message response;
  if (recvfrom(s, &response, sizeof(message), 0, 0, 0) < 0) {
    perror("No reply received when trying to connect to server");
    exit(1);
  }

  close(s);

  return response;
}

message msg_request(char* ip, int port, machine_info* mach, char msg[BUFSIZE]) {
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
    perror("Error setting socket timeout parameter\n");
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
  server.sin_addr.s_addr = inet_addr(ip);
  server.sin_port = htons(port);

  if (sendto(s, &text_msg, sizeof(message), 0, (struct sockaddr *)&server, 
      (socklen_t)sizeof(struct sockaddr)) < 0) {
    perror("No chat active at this address, try again later\n");
    exit(1);
  }

  message response;
  if (recvfrom(s, &response, sizeof(message), 0, 0, 0) < 0) {
    perror("No reply received when trying to connect to server");
    exit(1);
  }

  close(s);

  return response;
}

void parse_incoming_cl(message m, machine_info* mach, struct sockaddr_in source,
    int s, char* host_ip, int host_port) {
  if (m.header.msg_type == JOIN_REQ) {
    //pack up a lovely response message
    message respond_join;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = JOIN_RES;
    header.status = TRUE;
    header.about = *mach;
    respond_join.header = header;

    sprintf(respond_join.content, "%s:%d", host_ip, host_port);

    if (sendto(s, &respond_join , sizeof(respond_join), 0, 
        (struct sockaddr*)&source, sizeof(source)) < 0) {
      perror("Error responding to client with host info");
      exit(1);
    }
  } else if (m.header.msg_type == MSG_REQ) {
    printf("%s\n", m.content);
  }
}

void* client_listen(void* input) {
  thread_params* params = (thread_params*)input;

  //store info on message source
  struct sockaddr_in source;
  socklen_t sourcelen = sizeof(source);

  message incoming;
  int done = FALSE;
  while (!done) {
    if (recvfrom(params->socket, &incoming, sizeof(message), 0, 
        (struct sockaddr*)&source, &sourcelen) < 0) {
      perror("Error listening to incoming messages as client");
      exit(1);
    }

    parse_incoming_cl(incoming, params->mach, source, params->socket, 
      params->host_ip, params->host_port);
  }

  pthread_exit(0);
}