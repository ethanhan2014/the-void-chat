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

void client_start() {
  // setup socket for listening
  int s = open_listener_socket(this_mach);

  // setup socket for heartbeat
  int hb = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in hb_receiver;
  bzero((char *) &hb_receiver, sizeof(hb_receiver));
  hb_receiver.sin_family = AF_INET;
  hb_receiver.sin_addr.s_addr = inet_addr(this_mach->ipaddr);
  hb_receiver.sin_port = htons(this_mach->portno-1); //assigns to random open port
  if (bind(hb, (struct sockaddr*) &hb_receiver, sizeof(hb_receiver)) < 0) {
    perror("Error binding listener socket");
    exit(1);
  }

  client_queue = (linkedList *) malloc(sizeof(linkedList));
  client_queue->length = 0;

  printf("%s joining a new chat on %s:%d, listening on %s:%d\n", this_mach->name, 
    this_mach->host_ip, this_mach->host_port, this_mach->ipaddr, this_mach->portno);
  message host_attempt = join_request(this_mach);

  //real host info is in contents of message if we did not contact leader
  if (!host_attempt.header.about.isLeader) {
    char ip_port[BUFSIZE];
    strcpy(ip_port, host_attempt.content);
    char* new_ip = strtok(ip_port, ":");
    int new_port = strtol(strtok(NULL, " "), 0, 10);

    strcpy(this_mach->host_ip, new_ip);
    this_mach->host_port = new_port;

    host_attempt = join_request(this_mach);
  }
  latestSequenceNum = host_attempt.header.about.current_sequence_num - 1;
  update_clients(this_mach, host_attempt.header.about);

  printf("Succeeded, current users:\n");
  print_users(this_mach);

  //we have leader info inside of host_attempt message now
  client_loop(s, hb);


  //*** CLEANUP ***//
  close(s); //close sockets
  close(hb);

  //clear queue if not empty
  node* curr = client_queue->head;
  node* next = NULL;
  while (curr != NULL) {
    next = curr->next;
    free(curr);
    curr = next;
  }
  free(client_queue);
}

void client_loop(int s, int hb) {
  //kick off a thread that is listening in parallel
  thread_params params;
  params.socket = s;
  params.sock_hb = hb;

  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, client_listen, &params)) {
    perror("Error making thread to parse incoming message");
    exit(1);
  }

  pthread_t printer_thread;
  if (pthread_create(&printer_thread, NULL, sortAndPrint, NULL)) {
    perror("Error making printer thread to process message queue");
    exit(1);
  }

  pthread_t hb_receiver;
  if (pthread_create(&hb_receiver, NULL, recv_clnt_hb, &params)) {
    perror("Error making heartbeat thread");
    exit(1);
  }

  pthread_t hb_checker;
  if (pthread_create(&hb_checker, NULL, check_hb, &params)) {
    perror("Error making hb checking thread");
    exit(1);
  }

  pthread_t input_thread;
  if (pthread_create(&input_thread, NULL, user_input, &params)) {
    perror("Error making user input thread");
    exit(1);
  }

  //loop waiting for trigger, then transform if necessary
  while (!client_trigger);
  if (client_trigger == 2) { //transform
    //first force kill user_input since it might be stuck on scanf or socket input
    pthread_cancel(input_thread);

    //remove leader
    remove_leader(this_mach);
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

void parse_incoming_cl(message m, struct sockaddr_in source, int s) {
  if (m.header.msg_type == JOIN_REQ) { //other client trying to connect
    //pack up a response message
    message respond_join;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = JOIN_RES;
    header.status = TRUE;
    header.about = *this_mach;
    respond_join.header = header;

    sprintf(respond_join.content, "%s:%d", this_mach->host_ip, 
      this_mach->host_port);

    if (sendto(s, &respond_join , sizeof(respond_join), 0, 
        (struct sockaddr*)&source, sizeof(source)) < 0) {
      perror("Error responding to client with host info");
      exit(1);
    }
  } else {
    addElement(client_queue, m.header.seq_num, "", m);

    // ack the message and let sequencer know it was received
    message ack;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = ACK;
    header.status = TRUE;
    header.about = *this_mach;
    ack.header = header;
    if (sendto(s, &ack , sizeof(ack), 0, 
        (struct sockaddr*)&source, sizeof(source)) < 0) {
      perror("Error responding to client with ACK");
      exit(1);
    }

    // above means we print out these messages, do any specific other operations
    if (m.header.msg_type == NEW_USER) {
      add_client(this_mach, m.header.about);
    } else if (m.header.msg_type == LEAVE) {
      remove_client_mach(this_mach, m.header.about);
    }
  }
}

int receive_message(int s, message* m, struct sockaddr_in* source, 
    machine_info* mach) {
  socklen_t sourcelen = sizeof(*source);
  if (recvfrom(s, m, sizeof(message), 0, (struct sockaddr*)source, 
      &sourcelen) < 0) {
    return FALSE;
  }

  return TRUE;
}

void* client_listen(void* input) {
  thread_params* params = (thread_params*)input;

  while (!client_trigger) {
    message incoming;
    struct sockaddr_in source;

    //wait for a message + update clients every time if message from leader
    if (receive_message(params->socket, &incoming, &source, this_mach) == FALSE) {
      error("Error listening for incoming messages");
    }

    //deal with the message
    parse_incoming_cl(incoming, source, params->socket); 
  }

  pthread_exit(0);
}

void* sortAndPrint() {
  while (!client_trigger) {
    //we simply find the next one in the sequence if we can
    //otherwise, we hold
    int i = 0;
    int found = FALSE;
    for (i = 0; i < client_queue->length; i++) {
      if (getElement(client_queue, i)->v == latestSequenceNum + 1) {
        found = TRUE;
        break;
      }
    }
    //we found the message
    if (found) {
      print_message(getElement(client_queue, i)->m);
      removeElement(client_queue, i);
      latestSequenceNum++;
    }
    else { //well, we didn't find it, so we gotta wait
      //printf("Message missing\n");
    }
  }
  pthread_exit(0);
}

void* recv_clnt_hb(void *param)
{
  thread_params* params = (thread_params*)param;
  client *this = &this_mach->others[0];

  message hb;
  struct sockaddr_in source;
  source.sin_family = AF_INET;

  while(!client_trigger) {
    if(receive_message(params->sock_hb, &hb, &source, this_mach) == FALSE) {
      error("Error receiving heartbeat message");
    }

    this->recv_count = this->send_count;

    hb.header.msg_type = ACK;
    hb.header.about = *this_mach;

    if (sendto(params->sock_hb, &hb, sizeof(message), 0, 
        (struct sockaddr *)&source, (socklen_t)sizeof(struct sockaddr)) < 0) {
      error("Error sending heartbeat message");
    }
  }
  pthread_exit(0);
}

void* check_hb(void *param)
{
  client *this = &this_mach->others[0];

  while(!client_trigger)
  {
    if(this->send_count - this->recv_count > 3)
    {
      client_trigger = 2;
      //TODO hold leader election...
    }
    waitFor(3);
    this->send_count++;
  }
  pthread_exit(0);
}

void* user_input(void *input) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //allows instant cancellation

  while(!client_trigger) {
    char user_in[BUFSIZE]; //get user input (messages)
    if (scanf("%s", user_in) == EOF) {
      //on ctrl-d (EOF), kill this program instead of interpreting input
      client_trigger = 1;
    } else {
      msg_request(this_mach, user_in);
    }
  }

  pthread_exit(0);
}
