#pragma once

#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSIZE 1024
#define MAX_CHAT_SIZE 20
#define TRUE 1
#define FALSE 0

//message types
#define MSG_REQ 3
#define MSG_RES 4
#define JOIN_REQ 5
#define JOIN_RES 6
#define ELECTION_REQ 7
#define ACK 8
#define NEWLEADER 9

// *** STRUCT FORMATS *** //
//for messages between machines
typedef struct client {
  char name[BUFSIZE];
  char ipaddr[BUFSIZE];
  int portno;
  int isLeader;
} client;
typedef struct machine_info {
  char ipaddr[BUFSIZE];   // ip address of member
  int portno;             // port number
  char name[BUFSIZE];     // member's name
  int isAlive;            // indicate alive or dead
  int isLeader;           // indicate whether it is leader
  client others[MAX_CHAT_SIZE]; // store other chatter info in here at all times
  int chat_size;          // number of other chatters
  char host_ip[BUFSIZE];  // current leader's ip
  int host_port;// current leader's port
} machine_info;
typedef struct msg_header {
  int timestamp;          // timestamp of sending out message
  int seq_num;            // sequence number proposed by sender
  int msg_type;           // response or chat message or leader election request
  int status;             //indicate whether it's ready to be delivered
  machine_info about;     // information about sender machine
} msg_header;
typedef struct messages {
  msg_header header;      //header information contains information about individual machine
  char content[BUFSIZE];  // contains the chatting information 
} message;

//for thread parameters
typedef struct thread_params {
  machine_info* mach; //relevant machine_info
  int socket; //relevant socket
} thread_params;


// *** FUNCTIONS *** //
//prints out a formatted list of users and their ip addresses
void print_users(machine_info* mach);

//use to parse command line input into ip and port parameters
//returns -1 when port number is invalid/not found at all, otherwise stores
//the given ip and port (or whatever part of the port could be interpreted)
//in ip and port parameters
int process_inputs(int argc, char const *argv[], char** ip, int* port);

//construct this executable's machine_info struct; involves determining this 
//machine's ip address that other clients can reach it at
//note that this does not set portno, which is assigned randomly when we make 
//this machine's listener port
machine_info get_machine_info(char const *name);

//updates add_to's client list by adding a client with add's information
void add_client_to_clientlist(machine_info* add_to, machine_info add);

//creates and binds a socket on the ip of the provided machine_info struct
//also updates provided machine_info with the port we assign to
//socket gets returned as an int 
int open_socket(machine_info* mach);

//changes update's client list to match source's
void update_clients(machine_info* update, machine_info source);


int sendPackets();
int receivePackets();
void error(char *msg);