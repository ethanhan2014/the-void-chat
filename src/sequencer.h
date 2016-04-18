#pragma once

#include "util.h"


linkedList *messagesQueue;
int currentSequenceNum;
int currentPlaceInQueue;

void sequencer_start(machine_info* mach);

//parse incoming message and perform proper response
void parse_incoming_seq(message m, machine_info* mach, struct sockaddr_in source,
  int s);

//enters main client loop, given this machine's info, socket s
void sequencer_loop(machine_info* mach, int s, int hb);

//send out message m to all clients in the master (leader's) client list
//(assumes mach is a leader)
void broadcast_message(message m, machine_info* mach);

// *** THREAD FUNCTIONS *** //
// listens on the provided socket
void* sequencer_listen(void* input);

void* sequencer_send_queue(void* input);

void* send_hb(void *param);

void* recv_hb(void *param);