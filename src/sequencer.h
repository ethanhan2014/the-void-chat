#pragma once

#include "util.h"


void sequencer_start(machine_info* mach);

//parse incoming message and perform proper response
void parse_incoming_seq(message m, machine_info* mach, struct sockaddr_in source,
  int s);

//enters main client loop, given this machine's info, socket s
void sequencer_loop(machine_info* mach, int s);

//send out message m to all clients in the master (leader's) client list
//(assumes mach is a leader)
void broadcast_message(message m, machine_info* mach);

// *** THREAD FUNCTIONS *** //
// listens on the provided socket
void* sequencer_listen(void* input);