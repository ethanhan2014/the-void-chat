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
  message host_attempt = find_host(ip, port, mach);

  //real host info is in contents of message if we did not contact leader
  if (!host_attempt.header.about.isLeader) {
    char ip_port[BUFSIZE];
    strcpy(ip_port, host_attempt.content);
    ip = strtok(ip_port, ":");
    port = strtol(strtok(NULL, " "), 0, 10);
    printf("%s:%d", ip, port);

    host_attempt = find_host(ip, port, mach);
  }

  //we have leader info inside of host_attempt message now
  mach->member_id = host_attempt.header.about.member_id;
  client_loop(ip, port, mach);
}

message find_host(char* ip, int port, machine_info* mach) {
  int s; //the socket
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("error making client socket");
    exit(1);
  }
  //give socket a timeout
  struct timeval thetime;
  thetime.tv_sec = 1;
  thetime.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &thetime, sizeof(thetime)) < 0) {
    perror("Error setting socket timeout parameter\n");
  }

  //pack up a lovely join request message
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
    perror("No chat active at this address, try again later");
    exit(1);
  }

  close(s);

  return response;
}

void* parse_incoming(void* input) {
  thread_params *params = input;

  if (params->incoming->header.msg_type == JOIN_REQ) {
    //pack up a lovely response message
    message respond_join;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = JOIN_RES;
    header.status = TRUE;
    header.about = *(params->mach);
    respond_join.header = header;
    strcat(respond_join.content, params->host_ip);
    strcat(respond_join.content, ":");
    sprintf(respond_join.content, "%d", params->host_port);

    struct sockaddr_in other_client;
    other_client.sin_family = AF_INET;
    other_client.sin_addr.s_addr = inet_addr(params->incoming->header.about.ipaddr);
    other_client.sin_port = htons(params->incoming->header.about.portno);

    if (sendto(params->socket, &respond_join , sizeof(respond_join), 0, 
        (struct sockaddr*)&other_client, sizeof(other_client)) < 0) {
      perror("Error responding to client with host info");
      exit(1);
    }
  }

  pthread_exit(0);
}

void client_loop(char* ip, int port, machine_info* mach) {
  int s; //the socket
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("error making client socket");
    exit(1);
  }

  //store info on message source
  struct sockaddr_in source;
  socklen_t sourcelen = sizeof(source);

  message incoming;
  int done = FALSE;
  while (!done) {
    if (recvfrom(s, &incoming, sizeof(incoming), 0, (struct sockaddr*)&source, 
        &sourcelen) < 0) {
      perror("Error listening to incoming messages");
      exit(1);
    }

    pthread_t incoming_thrd;
    thread_params params;
    params.incoming = &incoming;
    params.mach = mach;
    params.host_ip = ip;
    params.host_port = port;
    params.socket = s;
    if (pthread_create(&incoming_thrd, NULL, parse_incoming, &params)) {
      perror("Error making thread to parse incoming message");
      exit(1);
    }
  }


  close (s);
}