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
#define NEW_USER 10
#define QUIT 11

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

//creates and binds a socket on the ip of the provided machine_info struct
//also updates provided machine_info with the port we assign to
//socket gets returned as an int 
int open_socket(machine_info* mach);

//updates add_to's client list by adding a client with add's information
void add_client(machine_info* add_to, machine_info add);
//remove the client that corresponds to remove from update's client list
void remove_client(machine_info* update, machine_info remove);
//changes update's client list to match source's
void update_clients(machine_info* update, machine_info source);

//respond to a message with socket s, sending message m, to ip:port in source
//note that we consider this a response, so NO response is expected in return
void respond_to(int s, message* m, struct sockaddr_in source);

//provided m is a type of message that should be printed by the machine,
//prints the appropriate message based on that type
//Note that this only prints messages from leaders; produces an error otherwise
void print_message(message m);

//prints the given message as an error and exits the program
//always uses error code 1 to exit
//use when perror not useful
void error(char* m);


//we define a linked list that has a node with a pointer to the next node in the list
//it also contains the data for the message
typedef struct node {
  struct node *next;
  int v;
  char *value;
  message *m;
} node;


//the linked list struct will also contain data on the first element in the list and the length of the
typedef struct linkedList {
  node *head;
  int length;
} linkedList;

//this will add an element to the list
//this function always adds elements to the end of the list
int addElement(linkedList *l, int value, char *otherVal, message *m);

//this function will remove an element from the list at index i
//although this function should in theory work, I never did quite end up using it in my last project lol
//So to all future code writers of this project, use this at your own risk. Don't say I didn't warn you...
int removeElement(linkedList *l, int i);

//this will get an element from the list at index i
//if i is greater than the length, it will return NULL
node *getElement(linkedList *l, int i);

//this will return teh first element of the list
node *seeTop(linkedList *l);