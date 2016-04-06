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
  machine_info mach = get_machine_info(argv[1]);

  //start a new chat or figure out which one to join
  if (argc == 2) {
    mach.isLeader = TRUE;

    sequencer_start(&mach);
  } else {
    mach.isLeader = FALSE;
    strcpy(mach.host_ip, ip);
    mach.host_port = port;

    client_start(&mach);
  }
  
  return 0;
}




