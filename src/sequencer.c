#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "util.h"
#include "sequencer.h"

void sequencer_start(machine_info* mach) {
  int s = open_socket(mach); //open socket on desired ip and decide port

  /* ************************************* */
  /* ******create heartbeat socket ******* */
  int hb = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in hb_receiver;
  bzero((char *) &hb_receiver, sizeof(hb_receiver));
  hb_receiver.sin_family = AF_INET;
  hb_receiver.sin_addr.s_addr = inet_addr(mach->ipaddr);
  hb_receiver.sin_port = mach->portno-1; //assigns to random open port
  if (bind(hb, (struct sockaddr*) &hb_receiver, sizeof(hb_receiver)) < 0) {
    perror("Error binding listener socket");
    exit(1);
  }
  /* ************************************* */

  add_client(mach, *mach); //adding its self to client list
  messagesQueue = (linkedList *) malloc(sizeof(linkedList));
  messagesQueue->length = 0;
  currentSequenceNum = 0;
  currentPlaceInQueue = 0;
  printf("%s started a new chat, listening on %s:%d\n", mach->name, 
    mach->ipaddr, mach->portno);
  printf("Succeded, current users:\n");
  print_users(mach);
  printf("Waiting for other users to join...\n");

  sequencer_loop(mach, s, hb);
  
  close(s);
}

void sequencer_loop(machine_info* mach, int s, int hb) {
  //kick off a thread that is listening in parallel
  thread_params params;
  params.mach = mach;
  params.socket = s;
  params.sock_hb = hb;

  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, sequencer_listen, &params)) {
    perror("Error making thread to parse incoming message");
    exit(1);
  }

  pthread_t sender_thread;
  if (pthread_create(&sender_thread, NULL, sequencer_send_queue, &params)) {
    perror("Error making thread to send shit! We're screwed!");
    exit(1);
  }

  pthread_t hb_sender_thread;
  if (pthread_create(&hb_sender_thread, NULL, send_hb, &params)) {
    perror("Error making thread to send shit! We're screwed!");
    exit(1);
  }

  pthread_t hb_receiver_thread;
  if (pthread_create(&hb_receiver_thread, NULL, recv_hb, &params)) {
    perror("Error making thread to send shit! We're screwed!");
    exit(1);
  }

  //loop waiting for user input
  int done = FALSE;
  while (!done) {
    char input[BUFSIZE]; //get user input (messages)
    printf("Waiting for input\n");
    if (scanf("%s", input) == EOF) {
      //on ctrl-d (EOF), kill this program instead of interpreting input
      done = TRUE;

      //TODO tell other clients to begin leader election
      //do we really want to do this? Shouldn't the clients detect leader failure automatically and begin this on own?
    } else {
      message text_msg; //make a message to send out then call broadcast_message
      msg_header header;
      header.timestamp = (int)time(0);
      header.msg_type = MSG_REQ;
      header.status = TRUE;
      header.about = *mach;
      text_msg.header = header;
      sprintf(text_msg.content, "%s:: %s", mach->name, input);
      //broadcast_message(text_msg, mach);
      printf("Adding to message queue\n");
      if (input[0] == '0') {
        printf("Special character detected\n");
        addElement(messagesQueue, currentSequenceNum, "YES", text_msg);
      }
      else {
        printf("No special character\n");
        addElement(messagesQueue, currentSequenceNum, "NO", text_msg);
      }
      currentSequenceNum++;
    }
    // node *current = messagesQueue->head;
    // printf("Cleaning up the message queue\n");
    // for (i = 0; i < messagesQueue->length; i++) {
    //   //free each element
    //   printf("Freeing a node\n");
    //   node *temp = current;
    //   current = current->next;
    //   //TODO: free internal data structures in each node as needed
    //   free(temp);
    // }
    // messagesQueue->length = 0;
  }
  //we need to clean up our data structure for messages
  int i = 0;
  node *current = messagesQueue->head;
  for (i = 0; i < messagesQueue->length; i++) {
    //free each element
    node *temp = current;
    current = current->next;
    //TODO: free internal data structures in each node as needed
    free(temp);
  }
}

void parse_incoming_seq(message m, machine_info* mach, struct sockaddr_in source,
    int s) {
  if (m.header.msg_type == JOIN_REQ) {
    //broadcast a join message to notify all others
    message join_msg;
    join_msg.header.timestamp = (int)time(0);
    join_msg.header.msg_type = NEW_USER;
    join_msg.header.status = TRUE;
    join_msg.header.about = *mach;
    sprintf(join_msg.content, "NOTICE %s joined on %s:%d", 
      m.header.about.name, m.header.about.ipaddr, m.header.about.portno);
    broadcast_message(join_msg, mach);

    //update clientlist
    add_client(mach, m.header.about); 

    //respond to original client
    message response;
    response.header.timestamp = (int)time(0);
    response.header.msg_type = JOIN_RES;
    response.header.status = TRUE;
    mach->current_sequence_num = currentSequenceNum;
    response.header.about = *mach;
    respond_to(s, &response, source);
  } else if (m.header.msg_type == MSG_REQ) {
    //respond to the original message
    message response;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = MSG_RES;
    header.status = TRUE;
    header.about = *mach;
    response.header = header;
    respond_to(s, &response, source);

    //format msg to send out
    message text_msg;
    text_msg.header.timestamp = (int)time(0);
    text_msg.header.msg_type = MSG_REQ;
    text_msg.header.status = TRUE;
    text_msg.header.about = *mach;
    sprintf(text_msg.content, "%s:: %s", m.header.about.name, m.content);
    //broadcast_message(text_msg, mach); //send the msg out to everyone
    addElement(messagesQueue, currentSequenceNum, "NO", text_msg);
    currentSequenceNum++;
    printf("We are currently on sequence number: %d\n", currentSequenceNum);
  } else if (m.header.msg_type == QUIT) {
    //Note no direct response expected for this message from the quitting client
    //clients will update their client lists upon receiving the message
    remove_client(mach, m.header.about); //update leader's client list

    message quit_msg;
    quit_msg.header.timestamp = (int)time(0);
    quit_msg.header.msg_type = QUIT;
    quit_msg.header.status = TRUE;
    quit_msg.header.about = *mach;
    sprintf(quit_msg.content, "NOTICE %s left the chat or crashed",
      m.header.about.name);
    broadcast_message(quit_msg, mach);//send the msg out to everyone
  }
}

void broadcast_message(message m, machine_info* mach) {
  //now send out the message to every client
  int o; //the socket
  if ((o = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error making socket to send message");
    exit(1);
  }
  //give socket a timeout
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if (setsockopt(o, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("Error setting socket timeout parameter");
  }

  //setup host info to connect to/send message to
  struct sockaddr_in server;
  server.sin_family = AF_INET;

  int i;
  for (i = 0; i < mach->chat_size; i++) {
    client this = mach->others[i];

    //might be yourself; just print it then
    if (this.isLeader) {
      print_message(m);
    } else {
      server.sin_addr.s_addr = inet_addr(this.ipaddr);
      server.sin_port = htons(this.portno);

      if (sendto(o, &m, sizeof(message), 0, (struct sockaddr *)&server, 
          (socklen_t)sizeof(struct sockaddr)) < 0) {
        perror("Cannot send message to this client");
        exit(1);
      }

      //TODO MAKE CLIENT RESPOND - IF TIMEOUT, IS CLIENT DEAD? etc., send out leave message
      //how do we do this tho?
      //should we wait some amount of time and block? Doesn't that defeat the purpose of the program?

      /* ****************************************************
      * If sequencer don't receive the ack message, 
      * recvfrom() will block the thread until it receive it
      *     ????? 
      * *****************************************************/


      message reply;
      socklen_t server_len = sizeof(server);
      int n = 0;
      for (n = 0; n < 3; n++) {
        if (recvfrom(o, &reply, sizeof(message), 0, (struct sockaddr*)&server, &server_len) < 0) {
          printf("Failed to get response\n");
          if (sendto(o, &m, sizeof(message), 0, (struct sockaddr *)&server, 
          (socklen_t)sizeof(struct sockaddr)) < 0) {
           perror("Cannot send message to this client");
           exit(1);
      }
        }
        else {
          if(reply.header.msg_type == ACK) {
            printf("It's an ack alright\n");
          }
          printf("Got response\n");
          break;
        }
      }
    }
  }
}

void* sequencer_listen(void* input) {
  thread_params* params = (thread_params*)input;

  //store info on message source
  struct sockaddr_in source;
  socklen_t sourcelen = sizeof(source);

  message incoming;
  int done = FALSE;
  while (!done) {
    if (recvfrom(params->socket, &incoming, sizeof(message), 0, 
        (struct sockaddr*)&source, &sourcelen) < 0) {
      done = TRUE; //socket might get closed by ctrl-d signal; then kill thread
    } else {
      parse_incoming_seq(incoming, params->mach, source, params->socket);
    }
  }

  pthread_exit(0);
}

void* sequencer_send_queue(void* input) {
  thread_params* params = (thread_params*)input;

  int done = FALSE;

  while (!done) {

   //printf("Current messages in queue: %d\n", messagesQueue->length);
   int i = 0;
   //printf("Broadcasting messages\n");
   for (i = currentPlaceInQueue; i < messagesQueue->length; i++) {
     node *c = getElement(messagesQueue, i);
     c->m.header.seq_num = c->v;
     //if (c->value[0] != 'Y') {
       broadcast_message(getElement(messagesQueue, i)->m, params->mach);
     //}
   }
   currentPlaceInQueue = i;
 }
  pthread_exit(0);
}

void* send_hb(void *param)
{
  thread_params* params = (thread_params*)param;

  machine_info *mach = params->mach;

  message *hb = (message *)malloc(sizeof(message));

  hb->header.msg_type = ACK;
  hb->header.about = *mach;

  struct sockaddr_in hb_sender_addr;
  int hb_sender_len;

  while(1)
  {
    /*loop thru all member machines*/
    int i;
    for(i = 0; i < mach->chat_size; i++)
    {
      client this = mach->others[i];
      if(!this.isLeader)
      { 

        bzero((char *) &hb_sender_addr, sizeof(hb_sender_addr));
        hb_sender_addr.sin_family = AF_INET;
        hb_sender_addr.sin_addr.s_addr = inet_addr(this.ipaddr);

        /* *******************************************
        *
        *  We assume that every heartbeat port is message portno-1;
        *
        *  ********************************************/

        hb_sender_addr.sin_port = htons(this.portno-1);

        hb_sender_len = sizeof(hb_sender_addr);

        if (sendto(params->sock_hb, hb, sizeof(*hb), 0, (struct sockaddr *)&hb_sender_addr, 
          hb_sender_len) < 0) {
          error("Cannot send message to this client");
        }

        this.send_count++;

        if(this.send_count - this.recv_count > 3)
        {
          /*claim this member is dead*/
          //remove the member
          //send out message to update alive members' group list

          printf("we notice %s quitted or crashed\n", this.name);
        }
      }
    }
    waitFor(3); //every 3 second sending out heartbeat messages 
  }
  
  pthread_exit(0);
}

void* recv_hb(void *param)
{
  thread_params* params = (thread_params*)param;

  machine_info *mach = params->mach;
  message *hb = (message *)malloc(sizeof(message));
  struct sockaddr_in hb_sender_addr;
  int hb_sender_len;
  while(1)
  {

    if(recvfrom(params->sock_hb, hb, sizeof(*hb), 0, 
        (struct sockaddr*)&hb_sender_addr, (unsigned int *)&hb_sender_len) < 0) 
    {
      error("Cannot receive message");
    }

    int i;
    for(i=0; i<mach->chat_size; i++)
    {
      client this = mach->others[i];
      if(this.portno == hb->header.about.portno 
        && strcmp(this.ipaddr,hb->header.about.ipaddr)==0)
      {
        this.recv_count = this.send_count;
        break;
      }
    }

  }

  pthread_exit(0);
}