# CIS505 - Project 3 - Distributed Chat - Final Report
by Team The Void* Chat

#### Team member: Conor Ryan (conoryan), Kunal Malhotra (malhk), Ziyi Han (hanziyi)
-----

## Usage Instructions

* In the project directory, type ```cd src``` to enter source code folder.
* Type ```make``` to compile all source code. 
* To start a new chat group, type ```./dchat <NAME>```. Then it will print the ip address and port number it listens on. NAME is the name you use for group chat.
* To join an existing chat group, type ```./dchat <NAME> <IPADDR>:<PORT>```. It will print group information after it succeeds. IP Address and port number can be any group member's ip and port. NAME can be same as existing members.
* To quit the chat, type ```CTRL+D``` or ```CTRL+C```. Group leader will remove you from the group.
* Type ```make clean``` to clean up compiled files.
* Note that a single chat group can allow up to 20 group members and each chat message can contain a maximum size of 1024 bit.

-----

## Software Design Specification

#### constant variables
* BUFSIZE is the size of message content and MAX_CHAT_SIZE is size of chat group.
* MSG_REQ, MSG_RES and so on are message types used for parsing different messages.

#### Data structures 
* struct linkedList contains struct node and int length that indicates the total length of the list.

* struct node contains a pointer to the next node. Each node can store a received message, a value and a char.

* struct client is used for storing all information about other group members. It contains name, ip address, port number, isLeader indicator and two heartbeat counter for counting heartbeat signals.

* struct machine_info is used for storing all information about group member itself. Besides name, ip address, port number, isLeader, it will also contain an array of struct client to store all information about group members. chat_size shows current number of people in the group. host_ip and host_port is current leader's ip address and port number. current_sequence_num is current sequence number of message queue which is implemented by a struct linkedList.

* struct thread_params is used for passing created sockets variables to each thread.

#### Message Types


Every message sent out by group member will include a message header and content. Content will contains the chat content. Header will contains all information about the message such as timestamp, sender's information and message type. 

```c
#define BUFSIZE 1024
#define MAX_CHAT_SIZE 10
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
#define LEAVE 11


// *** STRUCT FORMATS *** //

//for messages between machines

typedef struct client {
  char name[BUFSIZE];
  char ipaddr[BUFSIZE];
  int portno;
  int isLeader;
  int recv_count; //used for heartbeat count
  int send_count; //used for heartbeat count
} client;


typedef struct machine_info {
  char ipaddr[BUFSIZE];   // ip address of member
  int portno;             // port number
  char name[BUFSIZE];     // member's name
  int isLeader;           // indicate whether it is leader
  client others[MAX_CHAT_SIZE]; // store other chatter info in here at all times
  int chat_size;          // number of other chatters
  char host_ip[BUFSIZE];  // current leader's ip
  int host_port;// current leader's port
  int current_sequence_num;
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
typedef struct linkedList linkedList;

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

typedef struct thread_params {
  int socket; //relevant listener socket
  int sock_hb; //socket we set up to listen for heartbeat
} thread_params;
```

-----


#### Module Design 

* dchat.c
This part first handles input parameters and filters out bad input. By checking input parameters, it creates sequencer or client accordingly. It  

*util.c util.h
This part first defines all constants we need for the project. We specify that 

* Sequencer.c Sequencer.h
This part will implement the totally ordered multicast with a sequencer. Sequencer will need to receive all messages from other member and assign a timestamp to each message.  

* Client.c Client.h
This part will handle sending and receiving of all chat messages. When sending out messages, clients need to send out timestamp and proposed sequence number. When receiving messages, clients first put messages in the hold back queue and can deliver it only after receiving its final timestamp. 

* Election


-----

#### Protocol Implementation Details

##### Message broadcasting, receiving and parsing (TODO)

##### Fully ordered multicast protocol with a central sequencer

The fully ordered multicast protocol works as follows. All messages arriving at the sequencer are assigned a sequeunce number as they arrive. The sequence number is assigned to each message based on the order it arrived in. It is incremented each time a message arrives.

A separate thread is responsible for broadcasting these messages to the clients. The broadcast is also done in the order of arrival.

Finally, all clients maintain state about what the current sequence number is. As messages arrive, clients look for the message in the arrived messages queue with the correct sequence number and print it. If the message does not exist, then the client waits for the message to arrive.

##### Failure Detection and Leader election

* For failutre detection, we created 2 threads in the sequencer and each client to implement a heartbeat checking system. Heartbeat port is always the message port number - 1. 

* For sequencer, one thread broadcasts heartbeat signals to all chat members in every 3 seconds and it increments the sending counter at same time. Another thread listens for heartbeat signals response from each client and it change receiving counter to current sending counter if it receives response. If the broadcast thread find the difference between two counter is larger than 3, sequencer can declare that client is crashed or quitted.

* For client, one thread listens for heartbeat signals from leader. Once it receives signal, it send back response and set receive counter same as send counter. Another thread increments send counter in every 3 seconds. If difference between two counters is larger than 3, it can declare sequencer is dead.

* When one of the clients finds out sequencer is dead, it first suspends all running threads and then it starts its election thread. Election thread first determines if it's the next leader by comparing its port number with highest port number in the group. If it is the next leader, it first broadcasts NEWLEADER messages to all other clients, then it kills all suspended threads and transforms into sequencer. If it's not the next leader, it first broadcasts ELECTION_REQ messages to those clients with higher port number and waits for NEWLEADER message. When other clients receive ELECTION_REQ messages, they suspend all running thread and start the election thread. When other clients receive NEWLEADER messages, they suspend all running threads, then update leader information and resume all threads after all actions. 


##### BELOW IS TEXT FROM LAST MILESTONE. USE IT IF NEEDED.

We will need a separate custom built UDP library which implements the ordering aspect. This will be the ordered multicast file. So that will be a separate file, sequencer. The sequencer will need to reformat messages with a message order number as well. So any messages sent to the sequencer will need a timestamp while any messages sent out of the sequencer simply need a sequence number. 

We will have a client program, which will also implement the leader aspects (as every client in a chat can become a leader; in other words, every leader/sequencer is just a special client). A separate header file will be needed for implementing the election protocol (Bully algorithm). The current leader can be stored on each client as an int, since each process will be given a unique ID on startup.

Every message should receive a response as this is how we will detect crashes. In order to detect failures, each client sends an ACK back to sequencer each time sequencer sends messages. The sequencer sends ACKs to clients that send messages to it. If a client connects to a client that is not the leader, that client will send the details of which process is the current leader, and it’ll attempt to connect there instead.

The sequencer will notify all clients when a new client has connected, allowing them to update a linked list of clients. Each time a client crashes, the sequencer detects this if the client fails to ack. It will send an update to all clients. The sequencer crash is detected also by a failure to ack. An election request is sent to all clients, and they will remove the sequencer from their linked lists. 

Each client will have at least two threads. One thread will listen to messages and create new threads when a message is received. Another thread will be responsible for sending messages. The leader will not spin off threads for group chat messages as we want to handle those sequentially. The leader will create new threads for sending messages to the clients.

------

#### Extra Credit

* Encryption (TODO)

* Traffic Control (TODO)

* Message Priority (TODO)

------
