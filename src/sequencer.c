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
  add_client_to_clientlist(mach, *mach); //adding its self to client list

  printf("%s started a new chat, listening on %s:%d\n", mach->name, 
    mach->ipaddr, mach->portno);
  printf("Succeded, current users:\n");
  print_users(mach);
  printf("Waiting for other users to join...\n");

  sequencer_loop(mach, s);
  
  close(s);
}

void sequencer_loop(machine_info* mach, int s) {
  //kick off a thread that is listening in parallel
  thread_params params;
  params.mach = mach;
  params.socket = s;

  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, sequencer_listen, &params)) {
    perror("Error making thread to parse incoming message");
    exit(1);
  }

  //loop waiting for user input
  int done = FALSE;
  while (!done) {
    char input[BUFSIZE]; //get user input (messages)
    if (scanf("%s", input) == EOF) {
      //on ctrl-d (EOF), kill this program instead of interpreting input
      done = TRUE;
    } else {
      message text_msg; //make a message to send out then call broadcast_message
      msg_header header;
      header.timestamp = (int)time(0);
      header.msg_type = MSG_REQ;
      header.status = TRUE;
      header.about = *mach;
      text_msg.header = header;
      sprintf(text_msg.content, "%s:: %s", mach->name, input);

      broadcast_message(text_msg, mach);
    }
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
    add_client_to_clientlist(mach, m.header.about); 

    //respond to original client
    message response;
    response.header.timestamp = (int)time(0);
    response.header.msg_type = JOIN_RES;
    response.header.status = TRUE;
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
    //send the msg out to everyone
    broadcast_message(text_msg, mach);
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
    perror("Error setting socket timeout parameter\n");
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