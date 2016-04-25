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
#include "client.h"

//main loop
int main(int argc, char const *argv[]) {
  //check for correct argument format
  if (argc != 2 && argc != 3) {
    printf("Incorrect number of arguments; dchat expects one or two.\n");
    exit(1);
  }

  //process input and search for possible semantic errors
  char* ip;
  int port = -1;
  if (process_inputs(argc, argv, &ip, &port) < 0) {
    printf("Incorrectly formatted ip/port; try: $ dchat name ip:port\n");
    exit(1);
  }

  //establish this machine's information
  this_mach = (machine_info*)malloc(sizeof(machine_info));
  set_machine_info(argv[1]);

  //init client_trigger to allow below loops to run
  client_trigger = 0;

  pthread_mutex_init(&msg_queue_lock, NULL);
  pthread_mutex_init(&group_list_lock, NULL);

  //start a new chat or figure out which one to join; INITIAL iteration
  if (argc == 2) {
    this_mach->isLeader = TRUE;

    sequencer_start();
  } else {
    this_mach->isLeader = FALSE;
    strcpy(this_mach->host_ip, ip);
    this_mach->host_port = port;

    client_start();
  }

  //after initial iteration, become sequencer if client_trigger was made 2
  //otherwise, we just go past and finish since it is 1 to indicate the user
  //exitted with ctrl-d
  if (client_trigger == 2) {
    printf("Changing to sequencer instead of client due to election\n\n");

    this_mach->isLeader = TRUE;
    strcpy(this_mach->host_ip,this_mach->ipaddr);
    this_mach->host_port = this_mach->portno;
    client_trigger = 0; //reset so sequencer loop can run
    sequencer_loop(sockets.socket, sockets.sock_hb);
  }

  free(this_mach);
  
  return 0;
}




