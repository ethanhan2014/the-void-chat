#pragma once

#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSIZE 1024
#define MAX_CHAT_SIZE 10
#define TRUE 1
#define FALSE 0

//** MESSAGE TYPES ** //
//a message request
#define MSG_REQ 3
//response to message request
#define MSG_RES 4
//join chat request sent by new client
#define JOIN_REQ 5
//join chat response sent to client by leader once it receives request
#define JOIN_RES 6
//instruction to client to start election process of new leader
#define ELECTION_REQ 7
//general ACK message used in many places to confirm receipt
#define ACK 8
//message used to indicate that there is a new leader and it is the machine that
//send this message
#define NEWLEADER 9
//message to indicate that a new user has been approved by leader and is in chat
#define NEW_USER 10
//message to indicate that a user has left according to leader
#define LEAVE 11


// *** STRUCT FORMATS *** //

//for messages between machines
typedef struct linkedList linkedList;
typedef struct client {
  char name[BUFSIZE];
  char ipaddr[BUFSIZE];
  int portno;
  int isLeader;

  int recv_count; //used for heartbeat count
  int send_count; //used for heartbeat count
} client;

//actual payload data
typedef struct machine_info {
  char ipaddr[BUFSIZE];   // ip address of member
  int portno;             // port number
  char name[BUFSIZE];     // member's name
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
  int sender_seq;         // number assigned by original sender to ensure delivery
} msg_header;

typedef struct messages {
  msg_header header;      //header information contains information about individual machine
  char content[BUFSIZE];  // contains the chatting information 
} message;

//linked list
typedef struct node {
  struct node *next;
  int v;
  char *value;
  message m;
} node;
struct linkedList {
  node *head;
  int length;
};

//for thread parameters
typedef struct thread_params {
  int socket; //relevant listener socket
  int sock_hb; //socket we set up to listen for heartbeat
} thread_params;


// GLOBAL VARIABLES //

machine_info* this_mach; //information about itself
thread_params sockets;   //store created sockets

pthread_mutex_t msg_queue_lock;

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
void set_machine_info(char const *name);

//creates and binds a socket on the ip of the provided machine_info struct
//also updates provided machine_info with the port we assign to
//socket gets returned as an int 
int open_listener_socket(machine_info* mach);

//updates add_to's client list by adding a client with add's information
//note this also checks to see if client is already present; will not add twice
void add_client(machine_info* add_to, machine_info add);
//remove the client that corresponds to remove from update's client list
void remove_client_mach(machine_info* update, machine_info remove);
//same as above but removes based on the given client
void remove_client_cl(machine_info* update, client remove);
//removes any client with isLeader = TRUE from update's client list
void remove_leader(machine_info* update);
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


// ** LINKED LIST FUNCTIONS ** //

//this will add an element to the list
//this function always adds elements to the end of the list
int addElement(linkedList *l, int value, char *otherVal, message m);

//this function will remove an element from the list at index idx (where the first is at 0 and the last is at length-1)
int removeElement(linkedList *l, int idx);

//this will get an element from the list at index i
//if i is greater than the length, it will return NULL
node *getElement(linkedList *l, int i);

//this will return the first element of the list
node *seeTop(linkedList *l);

// wait for certain number of seconds
void waitFor (unsigned int secs);