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
#include "client.h"
#include "sequencer.h"

void client_start() {
  sendSeqNum = 0;
  // setup socket for listening
  int s = open_listener_socket(this_mach);

  // setup socket for heartbeat
  int hb = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in hb_receiver;
  bzero((char *) &hb_receiver, sizeof(hb_receiver));
  hb_receiver.sin_family = AF_INET;
  hb_receiver.sin_addr.s_addr = inet_addr(this_mach->ipaddr);
  hb_receiver.sin_port = htons(this_mach->portno-1); //assigns to random open port
  if (bind(hb, (struct sockaddr*) &hb_receiver, sizeof(hb_receiver)) < 0) {
    perror("Error binding listener socket");
    exit(1);
  }

  client_queue = (linkedList *) malloc(sizeof(linkedList));
  client_queue->length = 0;
  outgoing_queue = (linkedList *) malloc(sizeof(linkedList));
  outgoing_queue->length = 0;
  temp_queue = (linkedList *) malloc(sizeof(linkedList));
  temp_queue->length = 0;

  printf("%s joining a new chat on %s:%d, listening on %s:%d\n", this_mach->name, 
    this_mach->host_ip, this_mach->host_port, this_mach->ipaddr, this_mach->portno);
  message host_attempt = join_request(this_mach);

  //real host info is in contents of message if we did not contact leader
  if (!host_attempt.header.about.isLeader) {
    char ip_port[BUFSIZE];
    strcpy(ip_port, host_attempt.content);
    char* new_ip = strtok(ip_port, ":");
    int new_port = strtol(strtok(NULL, " "), 0, 10);

    strcpy(this_mach->host_ip, new_ip);
    this_mach->host_port = new_port;

    host_attempt = join_request(this_mach);
  }
  latestSequenceNum = host_attempt.header.seq_num - 1;
  update_clients(this_mach, host_attempt.header.about);

  printf("Succeeded, current users:\n");
  print_users(this_mach);

  //election initialization
  hold_election = 0;
  pthread_mutex_init(&election_lock, NULL);
  pthread_mutex_init(&no_election_lock, NULL);

  //we have leader info inside of host_attempt message now
  client_loop(s, hb);


  //*** CLEANUP ***//
  if (client_trigger != 2) {
    close(s); //close sockets since we are dead now (not transforming)
    close(hb);
  }

  //clear queues if not empty
  /*node* curr = client_queue->head;
  node* next = NULL;
  while (curr != NULL) {
    next = curr->next;
    free(curr);
    curr = next;
  }
  free(client_queue);

  curr = outgoing_queue->head;
  next = NULL;
  while (curr != NULL) {
    next = curr->next;
    free(curr);
    curr = next;
  }
  free(outgoing_queue);

  curr = temp_queue->head;
  next = NULL;
  while (curr != NULL) {
    next = curr->next;
    free(curr);
    curr = next;
  }
  free(temp_queue);*/
}

void client_loop(int s, int hb) {
  //kick off a thread that is listening in parallel
  thread_params params;
  params.socket = s;
  params.sock_hb = hb;

  sockets = params;

  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, client_listen, &params)) {
    perror("Error making thread to parse incoming message");
    exit(1);
  }

  pthread_t printer_thread;
  if (pthread_create(&printer_thread, NULL, sortAndPrint, NULL)) {
    perror("Error making printer thread to process message queue");
    exit(1);
  }

  pthread_t hb_receiver;
  if (pthread_create(&hb_receiver, NULL, recv_clnt_hb, &params)) {
    perror("Error making heartbeat thread");
    exit(1);
  }

  pthread_t hb_checker;
  if (pthread_create(&hb_checker, NULL, check_hb, &params)) {
    perror("Error making hb checking thread");
    exit(1);
  }

  pthread_t input_thread;
  if (pthread_create(&input_thread, NULL, user_input, &params)) {
    perror("Error making user input thread");
    exit(1);
  }

  pthread_t msg_out_thread;
  if (pthread_create(&msg_out_thread, NULL, send_out_input, &params)){
    perror("Error making message sender thread");
    exit(1);
  }

  pthread_t election_thread;
  if (pthread_create(&election_thread, NULL, elect_leader, &params)){
    perror("Error making message sender thread");
    exit(1);
  }

  //loop waiting for trigger, then transform if necessary
  while (!client_trigger);
  if (client_trigger == 2) { //transform
    //must all thread to transform
    //transform happens after loop exit and we return to dchat
    pthread_cancel(listener_thread);
    pthread_cancel(printer_thread);
    pthread_cancel(hb_receiver);
    pthread_cancel(hb_checker);
    pthread_cancel(input_thread);
    pthread_cancel(msg_out_thread);
  }
}

message join_request(machine_info* mach) {
  int s; //the socket
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error making socket to send message");
    exit(1);
  }
  //give socket a timeout
  struct timeval timeout;
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("Error setting socket timeout parameter");
    exit(1);
  }

  //make message to send
  message init_conn;
  msg_header header;
  header.timestamp = (int)time(0);
  header.msg_type = JOIN_REQ;
  header.status = TRUE;
  header.about = *mach;
  init_conn.header = header;

  //setup host info to connect to/send message to
  char ip_copy[BUFSIZE]; //inet_addr changes ip(?)... use a copy of it to avoid
  strcpy(ip_copy, mach->host_ip); 
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip_copy);
  server.sin_port = htons(mach->host_port);

  if (sendto(s, &init_conn, sizeof(message), 0, (struct sockaddr *)&server, 
      (socklen_t)sizeof(struct sockaddr)) < 0) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  message response;
  if (receive_message(s, &response, NULL, mach) == FALSE) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  close(s);

  return response;
}

message msg_request(machine_info* mach, message m) {
  int s; //the socket
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Error making socket to send message");
    exit(1);
  }
  //give socket a timeout
  /*struct timeval timeout;
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("Error setting socket timeout parameter");
  }*/

  //setup host info to connect to/send message to
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(mach->host_ip);
  server.sin_port = htons(mach->host_port);

  if (sendto(s, &m, sizeof(message), 0, (struct sockaddr *)&server, 
      (socklen_t)sizeof(struct sockaddr)) < 0) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  message response;
  if (receive_message(s, &response, NULL, mach) == FALSE) {
    perror("No chat active at this address, try again later");
    exit(1);
  }

  close(s);

  return response;
}

void parse_incoming_cl(message m, struct sockaddr_in source, int s) {
  if (m.header.msg_type == JOIN_REQ) { //other client trying to connect
    //pack up a response message
    message respond_join;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = JOIN_RES;
    header.status = TRUE;
    header.about = *this_mach;
    respond_join.header = header;

    sprintf(respond_join.content, "%s:%d", this_mach->host_ip, 
      this_mach->host_port);

    if (sendto(s, &respond_join , sizeof(respond_join), 0, 
        (struct sockaddr*)&source, sizeof(source)) < 0) {
      perror("Error responding to client with host info");
      exit(1);
    }
  } else {

    pthread_mutex_lock(&msg_queue_lock);
    addElement(client_queue, m.header.seq_num, "", m);
    pthread_mutex_unlock(&msg_queue_lock);

    // ack the message and let sequencer know it was received
    message ack;
    msg_header header;
    header.timestamp = (int)time(0);
    header.msg_type = ACK;
    header.status = TRUE;
    header.about = *this_mach;
    ack.header = header;
    if (sendto(s, &ack , sizeof(ack), 0, 
        (struct sockaddr*)&source, sizeof(source)) < 0) {
      perror("Error responding to client with ACK");
      exit(1);
    }

    // above means we print out these messages, do any specific other operations
    if (m.header.msg_type == NEW_USER) {

      pthread_mutex_lock(&group_list_lock);
      add_client(this_mach, m.header.about);
      pthread_mutex_lock(&group_list_lock);

    } else if (m.header.msg_type == LEAVE) {

      pthread_mutex_lock(&group_list_lock);
      remove_client_mach(this_mach, m.header.about);
      pthread_mutex_lock(&group_list_lock);

    } else if (m.header.msg_type == ELECTION_REQ) {
      hold_election = 1;
      pthread_mutex_unlock(&no_election_lock);
    } else if (m.header.msg_type == NEWLEADER) {
      //hold on all threads
      remove_leader(this_mach);
      update_clients(this_mach,m.header.about);
      strcpy(this_mach->host_ip, m.header.about.ipaddr);
      this_mach->host_port = m.header.about.portno;
      //continue all threads
      hold_election = 0;

      pthread_mutex_unlock(&election_lock);
      pthread_mutex_unlock(&election_lock);
      pthread_mutex_unlock(&election_lock);
      pthread_mutex_unlock(&election_lock);

      print_users(this_mach);
    }
  }
}

int receive_message(int s, message* m, struct sockaddr_in* source, 
    machine_info* mach) {
  socklen_t sourcelen = sizeof(*source);
  if (recvfrom(s, m, sizeof(message), 0, (struct sockaddr*)source, 
      &sourcelen) < 0) {
    return FALSE;
  }

  return TRUE;
}

void* client_listen(void* input) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //allows instant cancellation

  thread_params* params = (thread_params*)input;

  while (!client_trigger) {
    message incoming;
    struct sockaddr_in source;
    
    while(hold_election)
    {
      pthread_mutex_lock(&election_lock);
    }

    //wait for a message + update clients every time if message from leader
    if (receive_message(params->socket, &incoming, &source, this_mach) == FALSE) {
      error("Error listening for incoming messages");
    }

    //deal with the message
    parse_incoming_cl(incoming, source, params->socket); 
  }

  pthread_exit(0);
}

void* sortAndPrint() {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //allows instant cancellation

  while (!client_trigger) {

    //now find the next one we want to print
    int i = 0;
    for (i = 0; i < client_queue->length; i++) {
      node* current = getElement(client_queue, i);
      if (current->v == latestSequenceNum + 1) { //got it
        //incr by 1 each iter then check to resend if necessary later (v > 1000)
        int n = 0;
        for (n = 0; n < temp_queue->length; n++) {
          getElement(temp_queue, n)->v++;
        }

        //now check to remove it from your sent queue
        for (n = 0; n < temp_queue->length; n++) {
          node* outgoing_msg = getElement(temp_queue, n);
          if (strcmp(this_mach->name, current->m.header.about.name) == 0 
              && strcmp(this_mach->ipaddr, current->m.header.about.ipaddr) == 0 
              && this_mach->portno == current->m.header.about.portno 
              && this_mach->isLeader == current->m.header.about.isLeader
              && current->m.header.sender_seq == outgoing_msg->m.header.sender_seq) {
            //we sent this one, and we will print it! remove from our sent queue
            pthread_mutex_lock(&msg_queue_lock);
            removeElement(temp_queue, n);
            pthread_mutex_unlock(&msg_queue_lock);
            n = temp_queue->length;
          }
          else if (outgoing_msg->v > 1000) {
            pthread_mutex_lock(&msg_queue_lock);
            addElement(outgoing_queue, 0, "NO", outgoing_msg->m);
            removeElement(temp_queue, n);
            pthread_mutex_unlock(&msg_queue_lock);
            n = temp_queue->length;
          }
        }

        //print it regardless of it we sent it initially
        print_message(current->m);

        pthread_mutex_lock(&msg_queue_lock);
        removeElement(client_queue, i);
        latestSequenceNum++;
        pthread_mutex_unlock(&msg_queue_lock);

        i = client_queue->length;
      }
    }
  }

  pthread_exit(0);
}

void* recv_clnt_hb(void *param)
{
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //allows instant cancellation

  thread_params* params = (thread_params*)param;
  client *this = &this_mach->others[0];

  message hb;
  struct sockaddr_in source;
  source.sin_family = AF_INET;

  while(!client_trigger) {

    while(hold_election)
    {
      pthread_mutex_lock(&election_lock);
    }

    if(receive_message(params->sock_hb, &hb, &source, this_mach) == FALSE) {
      error("Error receiving heartbeat message");
    }

    this->recv_count = this->send_count;

    hb.header.msg_type = ACK;
    hb.header.about = *this_mach;

    if (sendto(params->sock_hb, &hb, sizeof(message), 0, 
        (struct sockaddr *)&source, (socklen_t)sizeof(struct sockaddr)) < 0) {
      error("Error sending heartbeat message");
    }
  }
  pthread_exit(0);
}

void* check_hb(void *param)
{
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //allows instant cancellation

  client *this = &this_mach->others[0];

  while(!client_trigger)
  {
    while(hold_election)
    {
      pthread_mutex_lock(&election_lock);
    }

    if(this->send_count - this->recv_count > 3)
    {
      //trigger election
      hold_election = 1;
      pthread_mutex_unlock(&no_election_lock);
    }
    waitFor(3);
    this->send_count++;
  }
  pthread_exit(0);
}

void* user_input(void *input) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //allows instant cancellation

  while(!client_trigger) {

    char* user_in = (char*)malloc(BUFSIZE * sizeof(char)); //get user input (messages)
    size_t size = (size_t)BUFSIZE;
    if (getline(&user_in, &size, stdin) == -1) {
      //on ctrl-d (EOF), kill this program instead of interpreting input
      client_trigger = 1;
    } else {
      //form input as a msg
      message text_msg;
      msg_header header;
      header.timestamp = (int)time(0);
      header.msg_type = MSG_REQ;
      header.status = TRUE;
      header.about = *this_mach;
      text_msg.header = header;
      strcpy(text_msg.content, user_in);

      pthread_mutex_lock(&msg_queue_lock);
      addElement(outgoing_queue, 0, "NO", text_msg);
      pthread_mutex_unlock(&msg_queue_lock);
    }

    free(user_in);
  }

  pthread_exit(0);
}

void* send_out_input(void* input) {
  while (!client_trigger) {
    if (outgoing_queue->length > 0) {
      node* this = outgoing_queue->head;
      this->m.header.sender_seq = sendSeqNum;
      sendSeqNum++;

      pthread_mutex_lock(&msg_queue_lock);
      addElement(temp_queue, 0, "NO", this->m);
      pthread_unmutex_lock(&msg_queue_lock);

      msg_request(this_mach, this->m);

      pthread_mutex_lock(&msg_queue_lock);
      removeElement(outgoing_queue, 0);
      pthread_mutex_unlock(&msg_queue_lock);
    }
  }

  pthread_exit(0);
}

void* elect_leader(void* input)
{
  thread_params* params = (thread_params*)input;

  while (!client_trigger) {
    while(!hold_election)
    {
      pthread_mutex_lock(&no_election_lock); //no election
    }

    //remove leader
    remove_leader(this_mach);

    if(find_next_leader() == this_mach->portno) //claim leader
    {
      //update group information
      this_mach->isLeader = TRUE;
      strcpy(this_mach->host_ip,this_mach->ipaddr);
      this_mach->host_port = this_mach->portno;
      client_trigger = 2;
      int i;
      for(i=0;i<this_mach->chat_size;i++)
      {
        if(this_mach->others[i].portno == this_mach->portno)
        {
          this_mach->others[i].isLeader = TRUE;
        }
      }

      //broadcast results to all other clients
      message election_result;
      election_result.header.about = *this_mach;
      election_result.header.msg_type = NEWLEADER;
      election_result.header.seq_num = latestSequenceNum;
      currentSequenceNum = latestSequenceNum+1; //update seq var since we will become seq
      
      struct sockaddr_in dest;
      dest.sin_family = AF_INET;

      for(i=0;i<this_mach->chat_size;i++)
      {
        if(this_mach->others[i].portno != this_mach->portno)
        {
          dest.sin_addr.s_addr = inet_addr(this_mach->others[i].ipaddr);
          dest.sin_port = htons(this_mach->others[i].portno);

          if (sendto(params->socket, &election_result, sizeof(message), 0, (struct sockaddr *)&dest, 
              (socklen_t)sizeof(struct sockaddr)) < 0) {
            perror("Cannot send message to this client");
            exit(1);
          }
        }
      }
    }
    else
    {
      int found_leader = 0;

      message election_request;
      election_request.header.msg_type = ELECTION_REQ;
      election_request.header.about = *this_mach;

      struct sockaddr_in dest;
      socklen_t dest_len = sizeof(dest);
      dest.sin_family = AF_INET;

      //send election request to members with higher port number
      int i;
      for(i=0;i<this_mach->chat_size;i++)
      {
        if(this_mach->others[i].portno > this_mach->portno)
        {
          dest.sin_addr.s_addr = inet_addr(this_mach->others[i].ipaddr);
          dest.sin_port = htons(this_mach->others[i].portno);

          if (sendto(params->socket, &election_request, sizeof(message), 0, (struct sockaddr *)&dest, 
              (socklen_t)sizeof(struct sockaddr)) < 0) {
            perror("Cannot send message to this client");
            exit(1);
          }
        }
      }
      //listen for new leader information
      while(!found_leader)
      {
        message reply;
        if (recvfrom(params->socket, &reply, sizeof(message), 0, 
            (struct sockaddr*)&dest, &dest_len) < 0)
        {
          error("Cannot receive msg");
        }
        if(reply.header.msg_type == NEWLEADER)
        {
          found_leader = 1;
          //update information
          update_clients(this_mach,reply.header.about);
          strcpy(this_mach->host_ip, reply.header.about.ipaddr);
          this_mach->host_port = reply.header.about.portno;
          latestSequenceNum = reply.header.seq_num;

          //release all locks
          hold_election = 0;

          pthread_mutex_unlock(&election_lock);
          pthread_mutex_unlock(&election_lock);
          pthread_mutex_unlock(&election_lock);
          pthread_mutex_unlock(&election_lock);

          print_users(this_mach);
        }
      }
    }
  }
  pthread_exit(0);
}

/**
* This function will return the highest port number of client machine
**/
int find_next_leader()
{
  int next_leader;
  next_leader = this_mach->others[0].portno;
  int i;
  for(i=1;i<this_mach->chat_size;i++)
  {
    if(next_leader<this_mach->others[i].portno)
    {
      next_leader = this_mach->others[i].portno;
    }
  }
  return next_leader;
}
