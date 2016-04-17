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
  mach.current_sequence_num = 0; //(for now)

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

void add_client(machine_info* add_to, machine_info add) {
  client new;
  strcpy(new.name, add.name);
  strcpy(new.ipaddr, add.ipaddr);
  new.portno = add.portno;
  new.isLeader = add.isLeader;

  add_to->others[add_to->chat_size] = new;
  add_to->chat_size++;
}

void remove_client(machine_info* update, machine_info remove) {
  int i;
  for (i = 0; i < update->chat_size; i++) {
    client this = update->others[i];

    if (strcmp(this.name, remove.name) == 0 
        && strcmp(this.ipaddr, remove.ipaddr) == 0 
        && this.portno == remove.portno 
        && this.isLeader == remove.isLeader) {
      // match found; move all others up by one; for loop exits after this while
      while (i < update->chat_size) {
        update->others[i] = update->others[i+1];
        i++;
      }
      update->chat_size--;
    }
  }
}

void update_clients(machine_info* update, machine_info source) {
  memcpy(&(update->others), source.others, MAX_CHAT_SIZE * sizeof(client));
  update->chat_size = source.chat_size;
}

void respond_to(int s, message* m, struct sockaddr_in source) {
  if (sendto(s, m, sizeof(message), 0, (struct sockaddr*)&source,
      sizeof(source)) < 0) {
    perror("Error responding to message");
    exit(1);
  }
}

void print_message(message m) {
  if (!m.header.about.isLeader) {
    error("attempt to print a message not from the leader");
  }

  if (m.header.msg_type == MSG_REQ || m.header.msg_type == NEW_USER
      || m.header.msg_type == QUIT) {
    printf("%s\n", m.content);
  } else {
    error("attempt to print a message type that is not allowed");
  }
}

void error(char* m) {
  printf("Error: %s\n", m);
  exit(1);
}

int addElement(linkedList *l, int value, char *otherVal, message m) {
  //printf("Entering add\n");
  if (l == NULL) {
    //printf("List is null, exiting\n");
    return 1;
  }
  //printf("Going into main body\n");
  node *addNode = (node *)malloc(sizeof(node));
  addNode->v = value;
  addNode->value = otherVal;
  addNode->m = m;
  if (l->length == 0) {
    l->head = addNode;
    l->length++;
    return 0;
  }
  int i = 0;
  node *current = l->head;
  for (i = 0; i < l->length - 1; i++) {
    current = current->next;
  }
  current->next = addNode;
  l->length++;
  return 0;
}

int removeElement(linkedList *l, int i) {
  //printf("Removing\n");
  if (i >= l->length) {
    //printf("Invalid index\n");
    return 1;
  }
  if (i == 0) {
    node *c = l->head;
    l->head = l->head->next;
    free(c);
    l->length--;
    return 0;
  }
  int n = 0;
  node *current = l->head;
  for (n = 0; n < i; n++) {
    current = current->next;
  }
  node *removal = current->next;
  current = current->next->next;
  free(removal);
  l->length--;
  return 0;
}

node *getElement(linkedList *l, int i) {
  if (i >= l->length) {
    return NULL;
  }
  //printf("Past the null issue\n");
  int n = 0;
  node *current = l->head;
  //printf("starting search loop\n");
  for (n = 0; n < i; n++) {
    current = current->next;
  }
  //printf("Found the node\n");
  node *removal = current;
  return removal;
}

node *seeTop(linkedList *l) {
  return l->head;
}
