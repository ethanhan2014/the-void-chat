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
* 
We will need a separate custom built UDP library which implements the ordering aspect. This will be the ordered multicast file. So that will be a separate file, sequencer. The sequencer will need to reformat messages with a message order number as well. So any messages sent to the sequencer will need a timestamp while any messages sent out of the sequencer simply need a sequence number. 


We will have a client program, which will also implement the leader aspects (as every client in a chat can become a leader; in other words, every leader/sequencer is just a special client). A separate header file will be needed for implementing the election protocol (Bully algorithm). The current leader can be stored on each client as an int, since each process will be given a unique ID on startup.

Every message should receive a response as this is how we will detect crashes. In order to detect failures, each client sends an ACK back to sequencer each time sequencer sends messages. The sequencer sends ACKs to clients that send messages to it. If a client connects to a client that is not the leader, that client will send the details of which process is the current leader, and it’ll attempt to connect there instead.

The sequencer will notify all clients when a new client has connected, allowing them to update a linked list of clients. Each time a client crashes, the sequencer detects this if the client fails to ack. It will send an update to all clients. The sequencer crash is detected also by a failure to ack. An election request is sent to all clients, and they will remove the sequencer from their linked lists. 

Each client will have at least two threads. One thread will listen to messages and create new threads when a message is received. Another thread will be responsible for sending messages. The leader will not spin off threads for group chat messages as we want to handle those sequentially. The leader will create new threads for sending messages to the clients.

------
