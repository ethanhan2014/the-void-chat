# CIS505 - Project 3 - Distributed Chat
by Team The Void* Chat

-----

## Software Design Specification

#### Data structures 
* each member contains a linked list of all group members with one leader. 
* each member maintains a hold back queue of all incoming message.
* struct member will contains ip address, port number, name, isLeader, isAlive of the member.

#### Message Types

Every message sent out by group member will include a message header and content. Content will contains the chat content. Header will contains all information about the message such as timestamp, sender's information and message type. 

```c
#define BUFSIZE 1024;
#define TRUE 1;
#define FALSE 0;

#define MSG_REQ 3;
#define MSG_RES 4;

#define JOIN_REQ 5;
#define JOIN_RES 6;

#define ELECTION_REQ 7;
#define ACK 8;

#define NEWLEADER 9;

typedef struct messages {
  msg_header header,  //header information contains information about individual machine
  char content[BUFSIZE]  // contains the chatting information 
} message;

typedef struct message_header{
  int timestamp,             // timestamp of sending out message
  int seq_num,             // sequence number proposed by sender
  char msg_type[128],  // response or chat message or leader election request
  int status,            //indicate whether it's ready to be delivered
  machine_info about    // information about sender machine
} msg_header;

typedef struct machine_info{
  char ipaddr[BUFSIZE],               // ip address of member
  int portno,                         //port number
  char name[BUFSIZE],                 //member's name
  int member_id,                      // id number for the member in a group
  int isAlive,                        // indicate alive or dead
  int isLeader                        // indicate whether it’s leader
} machine_info;
```


-----


#### Module Design 

* main
This part will handle input parameters and setup of the socket. It will deal with establishing a connection to the server/other clients. Each member can find available port on its own. Once it is done with these things, it will create at least two threads. One is for listening for new messages and the other is for sending out messages. 

* Sequencer
This part will implement the totally ordered multicast with a sequencer. Sequencer will need to receive all messages from other member and assign a timestamp to each message. 

* Election
This part will perform the leader election process when failure of sequencer is detected by one member. By using Bully algorithm, we can elect the member with highest member_id in the group. Election process will start if one member can get response from sequencer. 

* Client
This part will handle sending and receiving of all chat messages. When sending out messages, clients need to send out timestamp and proposed sequence number. When receiving messages, clients first put messages in the hold back queue and can deliver it only after receiving its final timestamp. 

-----


#### Implementation Plan

* First, implement the UDP message sending and receiving.
* Second, implement the total ordered multicast protocol with sequencer.
* Third, implement leader election process

------