# CIS505 - Project 3 - Distributed Chat - Final Report
by Team The Void* Chat

#### Team members: Conor Ryan (conoryan), Kunal Malhotra (malhk), Ziyi Han (hanziyi)
-----

## Usage Instructions

* In the project directory, type ```cd src``` to enter source code folder.
* Type ```make``` to compile all source code. 
* To start a new chat group, type ```./dchat <NAME>```. Then it will print the ip address and port number it listens on. ```<NAME>``` is your name in this new group chat, which you will be leader of.
* To join an existing chat group, type ```./dchat <NAME> <IPADDR>:<PORT>```. It will print group information after it succeeds (like what IP and port this client listens on), or fail with an error. ```<IPADDR>``` and ```<PORT>``` can be the IP address and port number of any group member on the desired chat. ```<NAME>``` is still your desired name on this chat, which does not need to be unique.
* To send a message upon successful connection, just type your message and then press enter. You will see your message sent back once the leader receives it.
* To quit the chat as leader or client, type ```CTRL+D``` or ```CTRL+C```.
* Type ```make clean``` to clean up compiled files.

-----

## Software Design Specification

#### constant variables
* BUFSIZE is the size of message content and MAX_CHAT_SIZE is size of chat group. We restrict these to 1024 and 10 respecively, meaning messages can contain at most 1024 bits of information and there can be at most 10 people in a chat.
* MSG_REQ, MSG_RES, and so on are message types used for indicating the purpose of different incoming messages. Their specific descriptions are commented in ```util.h```, and also below in the description of messages.

#### Data structures 
* ```struct linkedList``` is a basic linked list with helper functions. It contains a pointer to the head of the list and an int to indicate the length of that list.

* ```struct node``` contains a pointer to the next node for the above linked list. Each node can store a received message, a value (meaning depends on context in program), and a char (meaning against varies).

* ```struct client``` is used for storing all information about other group members. It contains name, ip address, port number, isLeader indicator, and two heartbeat counters used for counting heartbeat signals and detecting leader/client failure. Every sequencer/leader maintains an array of this client info so that every machine knows at all times who is in the chat, regardless of failure.

* ```struct machine_info``` is used for storing all information about group member itself. This includes this machine's own name, ip address, port number, isLeader, as well as the array of ```client```s to store all information about group members. ```chat_size``` holds current number of people in the group. ```host_ip``` and ```host_port``` store the current leader's ip address and port number, as recognized by this client.

* ```struct thread_params``` is used for passing relevant information directly to certain threads we created. Sometimes the passed data holds nothing since that thread requires no information.

#### Message Types

Every message sent out by a group member includes a message header and content. Content contains the chat content that the user typed, while the header contains all information about the message such as timestamp, sender's information, and message type. 

```c
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

* dchat.c:
This part first handles input parameters and filters out bad input to a reasonable degree. After parsing input parameters, it turns changes into a sequencer or client instance of dchat accordingly. Sequencer instances call ```sequencer_start()``` inside of ```sequencer.c``` to begin listening for messages and waiting new clients, while client instances will call ```client_start()``` inside of ```client.c``` to find the proper sequencer, join that chat, and listen for messages.

* util.c/util.h:
This part first defines all constants, data structures, and universal variables or functions that sequencers or clients may make use of. It also contains helper functions relevant to the linkedList data structure library.

* sequencer.c/sequencer.h:
This part implements the totally ordered multicast with a sequencer. It allows for group member joining, and also detects client failure. It maintains ```sequencer_queue```, which is just the queue of messages that the sequencer received first from clients and sends out. Sequencers also allow for user input on their own end by appending to this queue.

* client.c/client.h:
This part handles sending and receiving of all chat messages. When a user types a message, this code first places messages into ```outgoing_queue```, which it then sends out from the head. After it believes it has sent out a message, it moves that message into ```temp_queue```, where the client will wait for the sequencer to send back any message that this client typed. If after some time the message from ```temp_queue``` is not returned, the client adds it back to ```outgoing_queue``` to ensure messages are not lost; otherwise, once that message is returned it gets removed since the message is complete. Finally, it also tracks ```client_queue``` in order to track any incoming message from the sequencer, and will traverse this queue looking for the next expected sequence number in order to ensure ordering. Messages remain in ```client_queue``` until they are found to have the desired sequence number. Note the additional functions for leader election, which halt the rest of the program when activated and handle the logic for leader elections should someone detect a leader failure.


-----

#### Protocol Implementation Details

##### Program startup, group starting/joining/leaving, message broadcasting/receiving, parsing (conoryan)

* The program startup and setup takes form in dchat.c. Here the program either becomes a client or sequencer initially, and then once the program leaves whichever of these initially occured, the program can reenter as a sequencer in the right conditions (following an election that a client won). Otherwise, this just jumps into the sequencer or client file.

* Clients join by pinging whatever IP:PORT the user gave initially and getting a response. When this fails there is clearly no chat active so the program terminates, otherwise if the response is from a non-leader, the message response contains the IP:PORT of the actual leader on that chat. So, the client parses that information (or just proceeds if it found the leader the first time) and tries to connect to the leader. Once the leader responds that the join was successful, the program just enters its main loop, waiting for user input and outside messages.

* User input and receiving messages are each separate threads. The user input thread appends to the outgoing queue of messages, while the thread to receive messages calls the appropriate functions given the type of message. Additionally, a separate thread also handles emptying the outgoing queue of messages in order to allow it to be halted during elections and avoid sending messages to a dead leader.

* Sequencers broadcast messages by just continuously emptying its message queue from the head, and sending to its list of clients. Its message queue gets populated by an identical listener thread.

##### Fully ordered multicast protocol with a central sequencer (malhk)

* The fully ordered multicast protocol works as follows. All messages arriving at the sequencer are assigned a sequeunce number as they arrive. The sequence number is assigned to each message based on the order it arrived in. It is incremented each time a message arrives.

* A separate thread is responsible for broadcasting these messages to the clients. The broadcast is also done in the order of arrival.

* Finally, all clients maintain state about what the current sequence number is. As messages arrive, clients look for the message in the arrived messages queue with the correct sequence number and print it. If the message does not exist, then the client waits for the message to arrive.

##### Failure Detection and Leader election (hanziyi)

* For failutre detection, we created 2 threads in the sequencer and each client to implement a heartbeat checking system. Heartbeat port is always the message port number - 1. 

* For sequencer, one thread broadcasts heartbeat signals to all chat members in every 3 seconds and it increments the sending counter at same time. Another thread listens for heartbeat signals response from each client and it change receiving counter to current sending counter if it receives response. If the broadcast thread find the difference between two counter is larger than 3, sequencer can declare that client is crashed or quitted.

* For client, one thread listens for heartbeat signals from leader. Once it receives signal, it send back response and set receive counter same as send counter. Another thread increments send counter in every 3 seconds. If difference between two counters is larger than 3, it can declare sequencer is dead.

* When one of the clients finds out sequencer is dead, it first suspends all running threads and then it starts its election thread. Election thread first determines if it's the next leader by comparing its port number with highest port number in the group. If it is the next leader, it first broadcasts NEWLEADER messages to all other clients, then it kills all suspended threads and transforms into sequencer. If it's not the next leader, it first broadcasts ELECTION_REQ messages to those clients with higher port number and waits for NEWLEADER message. When other clients receive ELECTION_REQ messages, they suspend all running thread and start the election thread. When other clients receive NEWLEADER messages, they suspend all running threads, then update leader information and resume all threads after all actions. 


#### Extra Credit

* Encryption (malhk)

* Traffic Control (conoryan): the implementation for this involves tracking sequencer-side extra information. The sequencer now tracks in its client list extra information, specifically the difference in time between (up to) each of the last 10 sent messages from each client. It updates this information any time it receives a message. After updating this information, it also checks if that client has been sending messages too fast based on the average of those last 10 int values; if the average time between messages is below a certain threshold, messages are coming too fast, so the sequencer sends out a new type of message (CTRL_SLOW) to that client telling it to slow down. This message contains some value by which the client must now space its messages. Upon receiving such a message, the client waits for that much time before sending out a message from its message queue, and keeps doing this until the leader tells the client it is safe to send quickly again. The leader detects this by sending a CTRL_STOP message to the client once it finds that it has been behaving nicely (i.e., the avg of last 10 messages is above the desired threshold). This allows for messages to not flood the leader, as desired.

* Message Priority (TODO)

------
