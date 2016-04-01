/* This file will finally generate the executable*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/queue.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <time.h>

#include "util.h"
#include "sequencer.h"

//return -1 when port number is invalid/not found at all, otherwise stores
//the given ip and port (or whatever part of the port could be interpreted)
//in ip and port parameters
int process_inputs(int argc, char const *argv[], char** ip, int* port) {
  if (argc == 3) {
    char ip_port[BUFSIZE];
    strcpy(ip_port, argv[2]);
    *ip = strtok(ip_port, ":");

    char* endptr;
    *port = strtol(strtok(NULL, " "), &endptr, 10);
    if (endptr == ip_port) {
      return -1;
    }
  }

  return 0;
}

machine_info get_machine_info(char const *name) {
  machine_info mach;

  //get ip address
  struct ifaddrs *address, *search;
  getifaddrs(&address);
  search = address;

  int done = FALSE;
  while (!done) {
    if (search->ifa_addr && search->ifa_addr->sa_family == AF_INET) {
      if (strcmp(search->ifa_name, "em1") == 0) {
        done = TRUE;
        struct sockaddr_in *info = (struct sockaddr_in *)search->ifa_addr;
        strcpy(mach.ipaddr, inet_ntoa(info->sin_addr));
      }
    }
    search = search->ifa_next;
  }
  freeifaddrs(address);

  //give default port no
  mach.portno = 5374; //may change obviously if occupied

  //give other info
  strcpy(mach.name, name);
  mach.isAlive = TRUE;
  mach.isLeader = FALSE; //(for now)

  return mach;
}

message find_host(char* ip, int port, machine_info mach) {
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
  header.about = mach;
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

//main loop
int main(int argc, char const *argv[]) {
  if (argc != 2 && argc != 3) {
    printf("Incorrect number of arguments; dchat expects one or two.\n");
    exit(1);
  }

  //process input and search for possible errors
  char* ip;
  int port = -1;
  if (process_inputs(argc, argv, &ip, &port) < 0) {
    printf("Incorrectly formatted ip/port; try: $ dchat name ip:port\n");
    exit(1);
  }

  //establish this machine's information
  machine_info mach = get_machine_info(argv[1]);

  //start a new chat or figure out which one to join
  if (argc == 2) {
    mach.isLeader = TRUE;
    mach.member_id = 1;

    printf("%s started a new chat, listening on %s:%d\n", mach.name, 
      mach.ipaddr, mach.portno);
    run(&mach);
  } else {
    //find ip/port of sequencer from given ip/port (if it exists!)
    printf("%s joining a new chat on %s:%d, listening on %s:%d\n", 
      mach.name, ip, port, mach.ipaddr, mach.portno);
    message response = find_host(ip, port, mach);
  }
  
  return 0;
}




