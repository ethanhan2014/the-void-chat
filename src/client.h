#pragma once

#include <stdio.h>
#pragma once

#include <stdlib.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "util.h"

linkedList *messageQueue;
linkedList *tempBuff;
int latestSequenceNum;
node *nextInSeq;

//kicks off client process
void client_start(machine_info* mach);

//sends a join request to mach's leader ip:port
//returns the response from the server
message join_request(machine_info* mach);

//sends a message (from the user input) to current leader
//returns the response from the server
message msg_request(machine_info* mach, char msg[BUFSIZE]);

//enters main client loop, given this machine's info, socket s we listen on
void client_loop(machine_info* mach, int s, int hb);

//parse given message and perform proper response
void parse_incoming_cl(message m, machine_info* mach, struct sockaddr_in source,
  int s);

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

void *recv_clnt_hb(void *param);
