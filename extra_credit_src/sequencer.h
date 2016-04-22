#pragma once

#include "util.h"


linkedList *sequencer_queue;
int currentSequenceNum;

void sequencer_start();

//parse incoming message and perform proper response
void parse_incoming_seq(message m, struct sockaddr_in source, int s);

//enters main client loop, given socket infos
void sequencer_loop(int s, int hb);

//send out message m to all clients in the master (leader's) client list
//(assumes mach is a leader)
void broadcast_message(message m);

//performs traffic control given a message m from some client 
void traffic_control(message m);

// *** THREAD FUNCTIONS *** //
// listens on the provided socket
void* sequencer_listen(void* input);

void* sequencer_send_queue(void* input);

void* send_hb(void *param);

void* recv_hb(void *param);