#pragma once

#define BUFSIZE 1024
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

typedef struct machine_info {
  char ipaddr[BUFSIZE];               // ip address of member
  int portno;                         // port number
  char name[BUFSIZE];                 // member's name
  int member_id;                      // id number for the member in a group
  int isAlive;                        // indicate alive or dead
  int isLeader;                       // indicate whether it’s leader
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

//for passing into a thread as a single parameter
typedef struct thread_params {
  message* incoming;
  machine_info* mach;
  char* host_ip;
  int host_port;
  int socket;
} thread_params;


int sendPackets();
int receivePackets();
void error(char *msg);