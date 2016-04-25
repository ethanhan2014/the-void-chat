#pragma once

#include <stdio.h>
#pragma once

#include <stdlib.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "util.h"

linkedList *client_queue;
int latestSequenceNum;
int sendSeqNum;

linkedList* outgoing_queue;
linkedList* temp_queue;

//shared value - many threads react when this changes to TRUE
//when it becomes 1, all threads end and the process dies
//when it becomes 2, all threads but the main one which becomes a sequencer
int client_trigger;

float slow_factor;

//locks
pthread_mutex_t election_lock; //lock up thread
pthread_mutex_t no_election_lock; //lock up election thread
int hold_election; //0-no election 1-holding election

//kicks off client process
void client_start();

//sends a join request to mach's leader ip:port
//returns the response from the server
message join_request(machine_info* mach);

//sends a message (from the user input) to current leader
//returns the response from the server
message msg_request(machine_info* mach, message m);

//enters main client loop, given this machine's info, socket s we listen on
void client_loop(int s, int hb);

//parse given message and perform proper response
void parse_incoming_cl(message m, struct sockaddr_in source, int s);

//receives a message on given socket s, stores it in m, and stores sender info
//in source (make source NULL if you don't care about this info)
//updates mach's client list if message came from leader (to match leader's)
//returns TRUE if successful in getting message, FALSE on error (socket closed, 
//for example)
int receive_message(int s, message* m, struct sockaddr_in* source, 
  machine_info* mach);

// *** THREAD FUNCTIONS *** //
// listens on the provided socket
void* client_listen(void* input);

void* sortAndPrint();

void* recv_clnt_hb(void *param);

void* check_hb(void *param);

void* user_input(void *input);

void* send_out_input(void* input);

//Election methods
void* elect_leader(void* input);

int find_next_leader();