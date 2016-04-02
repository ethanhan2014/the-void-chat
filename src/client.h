#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "util.h"

void client_start(char* ip, int port, machine_info* mach);
message find_host(char* ip, int port, machine_info* mach);
void client_loop(char* ip, int port, machine_info* mach);

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