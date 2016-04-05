#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "util.h"

//kicks off client process
void client_start(char* ip, int port, machine_info* mach);

//sends a join request to the given ip:port
//returns the response from the server
message join_request(char* ip, int port, machine_info* mach);

//sends a message (from the user input) to given ip:port
//returns the response from the server
message msg_request(char* ip, int port, machine_info* mach, char msg[BUFSIZE]);

//enters main client loop, given leader ip:port, this machine's info, socket s
void client_loop(char* ip, int port, machine_info* mach, int s);

//parse incoming message and perform proper response
void parse_incoming_cl(message m, machine_info* mach, struct sockaddr_in source,
  int s, char* host_ip, int host_port);

// *** THREAD FUNCTIONS *** //
// listens on the provided socket
void* client_listen(void* input);


/*This is a basic API from class notes for client part*/
/*
int init_process();  //start to listen on random port and ip address

int joinGroup(char *ipaddr, int port);  //send request to join the group

int multicast(message *m, group g);    // send the message to all members of group g
 
int deliver(message *m);               // delivers the message to the recipient process
 
int sender(message *m);                // unique identifier of the process that send the message

int group(message *m);                 // unique identifier of the group which the message m was sent

void print_help();                     //print help information
*/