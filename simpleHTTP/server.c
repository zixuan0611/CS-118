#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>

#include "constant.h"
#include "protocol.h"

FILE* filefd = NULL;
int sockfd;
unsigned short seqnum;
unsigned short acknum;
unsigned short cwnd;
unsigned short ssth;
char filename[10];

void handleSignals(int signum) {
  printf("ERROR: Received signal %d. Server closed\n", signum);
  if (filefd != NULL) {
    fclose(filefd);
    filefd = fopen(filename, "wb");
    fprintf(filefd, "INTERRUPT\n");
    fclose(filefd);
  }
  close(sockfd);
  exit(0);
}

FILE* createFile(const unsigned int count) {
  memset(filename, '\0', sizeof(filename));
  sprintf(filename, "%d.file", count);
  FILE* filefd = fopen(filename, "wb");
//  FILE* filefd = fopen("test2", "wb");
  return filefd;
}

bool threeWayHandshake(int sockfd, const struct sockaddr_in clientaddr, struct Packet recv_packet){
  if (!recv_packet.header.syn) {
      return false;
  }
  printout_recv(recv_packet);

  // Server receives a SYN packet
  srand(time(0));
  seqnum = rand() % 5000;
  acknum = recv_packet.header.seqnum + 1;
  cwnd = recv_packet.header.cwnd;
  ssth = recv_packet.header.ssth;

  // Server sends back a SYN-ACK packet
  sendSynAck(sockfd, clientaddr, seqnum, acknum, cwnd, ssth);

  return true;
}

bool handleCloseConnnectionFromClient(const int sockfd, const struct sockaddr_in clientaddr, const struct Packet recv_packet){
  //seqnum = recv_packet.header.acknum;
  acknum = recv_packet.header.seqnum + 1;
  cwnd = recv_packet.header.cwnd;
  ssth = recv_packet.header.ssth;
  if (recv_packet.header.fin) {
    return respondToFin(sockfd, clientaddr, seqnum, acknum, cwnd, ssth);
  }
  return false;
}


int main(int argc, char** argv){
  // Invalid arguments
  if (argc != 2) {
    perror("ERROR: Invalid arguments\nUSAGE: ./server <port_number>\n");
    exit(1);
  }
  const unsigned int PORT = strtol(argv[1], NULL, 10);

  // Handle signals
  signal(SIGTERM, handleSignals);
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, handleSignals);

  struct sockaddr_in serveraddr;
  struct sockaddr_in clientaddr;

  socklen_t addrlen = sizeof(struct sockaddr_in);

  if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("ERROR: socket");
    exit(1);
  }

  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(PORT);
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(serveraddr.sin_zero, '\0', sizeof(serveraddr.sin_zero));

  if ((bind(sockfd, (struct sockaddr*) &serveraddr, sizeof(struct sockaddr)) == -1)) {
    perror("ERROR: bind");
    exit(1);
  }
  unsigned int conn_count = 0; //Connection counter

  // Server runs forever
  while(1) {
    memset(&clientaddr, '\0', sizeof(clientaddr));

    //==========Server "listens" on connections==========
    struct Packet recv_packet;
    if (recvfrom(sockfd, &recv_packet, PACKET_SIZE, 0,
      (struct sockaddr*) &clientaddr, &addrlen) < 0) {
      perror("ERROR: recvfrom");
      exit(1);
    }

    //==========Perform three-way handshaking==========
    if (threeWayHandshake(sockfd, clientaddr, recv_packet)==false){
      fprintf(stderr, "%s\n", "ERROR: Failed to perform handshaking");
      continue;
    }

    //==========Performing data transmission=========
    recv_packet = waitForPacketData(sockfd);
    conn_count++;
    filefd = createFile(conn_count);
    bool established = false;  // Establish connection, i.e. having some data
    while (recv_packet.data_size>0) {
      if (!established) {
        established = true;
      }

//      char str[recv_packet.data_size];
//      int i = 0;
//      for (; i<recv_packet.data_size; i++){
//        str[i] = recv_packet.data[i];
//      }
      fwrite(recv_packet.data, sizeof(char), recv_packet.data_size, filefd);

//      fprintf(filefd, "%s", str);
      //seqnum = recv_packet.header.acknum;
      acknum = recv_packet.header.seqnum + recv_packet.data_size;
      cwnd = recv_packet.header.cwnd;
      ssth = recv_packet.header.ssth;
      sendAck(sockfd, clientaddr, seqnum, acknum, cwnd, ssth);
      // Expect new data
      recv_packet = waitForPacketData(sockfd);
    }

    //=========Perform FIN closing connection requested from client side===========
    bool result = handleCloseConnnectionFromClient(sockfd, clientaddr, recv_packet);

    //=========Perform closing connection from the server side (due to TIMEOUT)===========
    if (!result && established) {
      closeConnection(sockfd, clientaddr, seqnum, acknum, cwnd, ssth);
    }

    //=========Closing file===========
    fclose(filefd);
    filefd = NULL;
  }
  return 0;
}
