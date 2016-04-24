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

void sequencer_start() {
  // setup socket for listening
  int s = open_listener_socket(this_mach);

  // setup socket for heartbeat
  int hb = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in hb_receiver;
  bzero((char *) &hb_receiver, sizeof(hb_receiver));
  hb_receiver.sin_family = AF_INET;
  hb_receiver.sin_addr.s_addr = inet_addr(this_mach->ipaddr);
  hb_receiver.sin_port = htons(this_mach->portno-1); 
  if (bind(hb, (struct sockaddr*) &hb_receiver, sizeof(hb_receiver)) < 0) {
    perror("Error binding listener socket");
    exit(1);
  }
  add_client(this_mach, *this_mach); //adding its self to client list


  sequencer_loop(s, hb);

/*******************************
  *
  * The part below caused potential segmentation fault. 
  * TODO:: needs a fix here
  *
  ********************************/
  
  // // ** CLEANUP ** //
  // close(s);
  // close(hb);

  // node* curr = sequencer_queue->head;
  // node* next = NULL;
  // while (sequencer_queue->length > 0 && curr != NULL) {
  //   next = curr->next;
  //   free(curr);
  //   curr = next;
  // }

  // free(sequencer_queue);
}

void sequencer_loop(int s, int hb) {
  //initialization
  sequencer_queue = (linkedList *) malloc(sizeof(linkedList));
  sequencer_queue->length = 0;
  currentSequenceNum = 0;

  //print out users
  printf("%s started a new chat, listening on %s:%d\n", this_mach->name, 
    this_mach->ipaddr, this_mach->portno);
  printf("Succeded, current users:\n");
  print_users(this_mach);
  printf("Waiting for other users to join...\n");

  //kick off a thread that is listening in parallel
  thread_params params;
  params.socket = s;
  params.sock_hb = hb;

  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, sequencer_listen, &params)) {
    perror("Error making thread to parse incoming message");
    exit(1);
  }

  pthread_t sender_thread;
  if (pthread_create(&sender_thread, NULL, sequencer_send_queue, &params)) {
    perror("Error making sender thread");
    exit(1);
  }

  pthread_t hb_sender_thread;
  if (pthread_create(&hb_sender_thread, NULL, send_hb, &params)) {
    perror("Error making heartbeat sender thread");
    exit(1);
  }

  pthread_t hb_receiver_thread;
  if (pthread_create(&hb_receiver_thread, NULL, recv_hb, &params)) {
    perror("Error making heartbeat receiver thread");
    exit(1);
  }

  //loop waiting for user input
  int done = FALSE;
  while (!done) {
    char input[BUFSIZE]; //get user input (messages)
    if (fgets(input, BUFSIZE, stdin) == NULL) {
      //on ctrl-d (EOF), kill this program instead of interpreting input
      done = TRUE;
    } else {
      message text_msg;
      msg_header header;
      header.timestamp = (int)time(0);
      header.msg_type = MSG_REQ;
      header.status = TRUE;
      header.about = *this_mach;
      header.seq_num = currentSequenceNum;
      text_msg.header = header;
      sprintf(text_msg.content, "%s:: %s", this_mach->name, input);

      addElement(sequencer_queue, currentSequenceNum, "NO", text_msg);
      currentSequenceNum++;
    }
  }
}

void parse_incoming_seq(message m, struct sockaddr_in source, int s) {
  if (m.header.msg_type == JOIN_REQ) {
    //broadcast a join message to notify all others
    message join_msg;
    join_msg.header.timestamp = (int)time(0);
    join_msg.header.msg_type = NEW_USER;
    join_msg.header.status = TRUE;
    join_msg.header.about = m.header.about;
    join_msg.header.seq_num = currentSequenceNum;
    sprintf(join_msg.content, "NOTICE %s joined on %s:%d\n", 
      m.header.about.name, m.header.about.ipaddr, m.header.about.portno);

    add_client(this_mach, m.header.about); //update clientlist
    addElement(sequencer_queue, currentSequenceNum, "NO", join_msg); //update msgs
    currentSequenceNum++;

    //respond to original client
    message response;
    response.header.timestamp = (int)time(0);
    response.header.msg_type = JOIN_RES;
    response.header.status = TRUE;
    this_mach->current_sequence_num = currentSequenceNum;
    response.header.about = *this_mach;
    respond_to(s, &response, source);
  } else if (m.header.msg_type == MSG_REQ) {
    //respond to the original message
    message response;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = MSG_RES;
    header.status = TRUE;
    header.about = *this_mach;
    response.header = header;
    respond_to(s, &response, source);

    //format msg to send out
    message text_msg;
    text_msg.header.timestamp = (int)time(0);
    text_msg.header.msg_type = MSG_REQ;
    text_msg.header.status = TRUE;
    text_msg.header.about = m.header.about;
    text_msg.header.sender_seq = m.header.sender_seq;
    text_msg.header.seq_num = currentSequenceNum;
    sprintf(text_msg.content, "%s:: %s", m.header.about.name, m.content);

    //add to queue to send out
    addElement(sequencer_queue, currentSequenceNum, "NO", text_msg);
    currentSequenceNum++;
  }
}

void broadcast_message(message m) {
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
  for (i = 0; i < this_mach->chat_size; i++) {
    client this = this_mach->others[i];

    //might be yourself; just print it then
    if (this.isLeader) {
      print_message(m);
    } else if (m.header.msg_type == NEW_USER 
        && strcmp(m.header.about.name, this.name) == 0 
        && strcmp(m.header.about.ipaddr, this.ipaddr) == 0 
        && m.header.about.portno == this.portno 
        && m.header.about.isLeader == this.isLeader) { 
      //might be the client who just joined! (skip it)
      //notifying of own join is bad (TRUST ME)
    } else {
      server.sin_addr.s_addr = inet_addr(this.ipaddr);
      server.sin_port = htons(this.portno);

      if (sendto(o, &m, sizeof(message), 0, (struct sockaddr *)&server, 
          (socklen_t)sizeof(struct sockaddr)) < 0) {
        perror("Cannot send message to this client");
        exit(1);
      }

      message reply;
      socklen_t server_len = sizeof(server);
      int n;
      for (n = 0; n < 3; n++) {
        if (recvfrom(o, &reply, sizeof(message), 0, (struct sockaddr*)&server, 
            &server_len) < 0) {
          if (sendto(o, &m, sizeof(message), 0, (struct sockaddr *)&server, 
              (socklen_t)sizeof(struct sockaddr)) < 0) {
           perror("Cannot send message to this client");
           exit(1);
          }
        }
        else {
          break;
        }
      }
    }
  }

  close(o);
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
      parse_incoming_seq(incoming, source, params->socket);
    }
  }

  pthread_exit(0);
}

void* sequencer_send_queue(void* input) {
  while (1) {
    if (sequencer_queue->length > 0) {
      broadcast_message(sequencer_queue->head->m);
      removeElement(sequencer_queue, 0);
    }
  }

  pthread_exit(0);
}

void* send_hb(void *param)
{
  thread_params* params = (thread_params*)param;

  message hb;
  hb.header.msg_type = ACK;
  hb.header.about = *this_mach;

  struct sockaddr_in hb_sender_addr;

  while (1) {
    //loop through all member machines
    int i;
    for (i = 0; i < this_mach->chat_size; i++){
      client *this = &this_mach->others[i];
      if(!this->isLeader) {
        bzero((char *) &hb_sender_addr, sizeof(hb_sender_addr));
        hb_sender_addr.sin_family = AF_INET;
        hb_sender_addr.sin_addr.s_addr = inet_addr(this->ipaddr);

        //we assume that every heartbeat port is message portno-1
        hb_sender_addr.sin_port = htons(this->portno-1);

        if (sendto(params->sock_hb, &hb, sizeof(message), 0, 
          (struct sockaddr *)&hb_sender_addr,(socklen_t)sizeof(struct sockaddr)) < 0) 
        {
          error("Cannot send hb message to this client");
        }

        this->send_count++;
        if(this->send_count - this->recv_count > 3) {
          //this member is now dead
          //make message to send out notifying that the client left
          message leave_notice;
          leave_notice.header.timestamp = (int)time(0);
          leave_notice.header.msg_type = LEAVE;
          leave_notice.header.status = TRUE;
          leave_notice.header.about = *this_mach;
          leave_notice.header.seq_num = currentSequenceNum;
          sprintf(leave_notice.content, "NOTICE %s left the chat or crashed",
            this->name);
          addElement(sequencer_queue, currentSequenceNum, "NO", leave_notice);
          currentSequenceNum++;

          //then remove the member
          remove_client_cl(this_mach, *this);
        }
      }
    }
    waitFor(3); //send out heartbeat message every 3 seconds 
  }
  
  pthread_exit(0);
}

void* recv_hb(void *param)
{
  thread_params* params = (thread_params*)param;

  message hb;
  hb.header.msg_type = ACK;
  hb.header.about = *this_mach;

  struct sockaddr_in hb_sender_addr;
  socklen_t hb_sender_len;

  while(1)
  {

    if (recvfrom(params->sock_hb, &hb, sizeof(message), 0, 
          (struct sockaddr*)&hb_sender_addr, &hb_sender_len) < 0)
    {
      error("Cannot receive hb");
    }

    int i;
    for (i = 0; i < this_mach->chat_size; i++)
    {
      client *this = &this_mach->others[i];
      if(this->portno == hb.header.about.portno 
        && strcmp(this->ipaddr,hb.header.about.ipaddr)==0)
      {
        this->recv_count = this->send_count;
        break;
      }
    }

  }

  pthread_exit(0);
}