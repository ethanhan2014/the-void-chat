#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <ifaddrs.h>

#include "util.h"

void print_users(machine_info* mach) {
  int i;
  for (i = 0; i < mach->chat_size; i++) {
    client next = mach->others[i];
    (next.isLeader ? printf("%s %s:%d (Leader)\n", next.name, next.ipaddr, 
      next.portno) : printf("%s %s:%d\n", next.name, next.ipaddr, 
      next.portno));
  }
}

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

  //give other info
  strcpy(mach.name, name);
  mach.isAlive = TRUE;
  mach.isLeader = FALSE; //(for now)
  mach.chat_size = 0;

  return mach;
}

int open_socket(machine_info* mach) {
  //start listening on your ip/port, and change portno if not open
  int s; // the socket
  if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error opening listener socket");
    exit(1);
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(mach->ipaddr);
  server.sin_port = 0; //assigns to random open port

  if (bind(s, (struct sockaddr*) &server, sizeof(server)) < 0) {
    perror("Error binding listener socket");
    exit(1);
  }

  //determine port number we used
  struct sockaddr_in sock_info;
  socklen_t info_len = sizeof(sock_info);
  if(getsockname(s, (struct sockaddr *)&sock_info, &info_len) != 0) {
    perror("Error detecting dynamically assigned listener port");
  }
  mach->portno = ntohs(sock_info.sin_port);

  return s;
}

void add_client_to_clientlist(machine_info* add_to, machine_info add) {
  client new;
  strcpy(new.name, add.name);
  strcpy(new.ipaddr, add.ipaddr);
  new.portno = add.portno;
  new.isLeader = add.isLeader;

  add_to->others[add_to->chat_size] = new;
  add_to->chat_size++;
}

void update_clients(machine_info* update, machine_info source) {
  memcpy(&(update->others), source.others, MAX_CHAT_SIZE * sizeof(client));
  update->chat_size = source.chat_size;
}