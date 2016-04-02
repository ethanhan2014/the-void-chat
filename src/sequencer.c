#include "sequencer.h"
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>

void print_users(machine_info* mach) {
  (mach->isLeader ? printf("%s %s:%d (Leader)\n", mach->name, mach->ipaddr, 
    mach->portno) : printf("%s %s:%d\n", mach->name, mach->ipaddr, 
    mach->portno));
}

void sequencer_init(machine_info* mach) {
  //start listening on your ip/port, and change portno if not open
  int s; // the socket
  if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error opening socket for new chat");
    exit(1);
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(mach->ipaddr);
  server.sin_port = htons(mach->portno);

  //dynamically choose port number
  while (bind(s, (struct sockaddr*) &server, sizeof(server)) < 0) {
    mach->portno++;
    server.sin_port = htons(mach->portno);
  }

  printf("Succeded, current users\n");
  print_users(mach);
  printf("Waiting for other users to join...\n");

  struct sockaddr_in client;
  socklen_t clientlen = sizeof(client);

  int done = FALSE;
  while (!done) {
    message input;
    if (recvfrom(s, &input, sizeof(message), 0, (struct sockaddr*)&client, 
        &clientlen) < 0){
      perror("Error in sequencer receiving message");
      exit(1);
    }

    printf("%d\n", input.header.msg_type);

    message response;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = JOIN_RES;
    header.status = TRUE;
    header.about = *mach;
    response.header = header;
    if (sendto(s, &response , sizeof(response), 0, 
        (struct sockaddr*)&client, sizeof(client)) < 0) {
      perror("Error responding to client");
      exit(1);
    }

    mach->member_id++; //new client will take old id of sequencer
  }
  
  close(s);
}