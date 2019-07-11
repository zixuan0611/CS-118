/*
Defines protocol of server-client connection
*/
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>

#include "packet.h"
#include "constant.h"

void printout_recv(const struct Packet packet) {
  char msg[128];
  memset(&msg, '\0', 128);
  sprintf(msg, "RECV %d %d %d %d ", packet.header.seqnum, packet.header.acknum,
          packet.header.cwnd, packet.header.ssth);
  if (packet.header.ack) {
    strcat(msg, "ACK ");
  }
  if (packet.header.syn) {
    strcat(msg, "SYN ");
  }
  if (packet.header.fin) {
    strcat(msg, "FIN ");
  }
  printf("%s\n", msg);
}

void printout_send(const struct Packet packet) {
  char msg[128];
  memset(&msg, '\0', 128);
  sprintf(msg, "SEND %d %d %d %d ", packet.header.seqnum, packet.header.acknum,
            packet.header.cwnd, packet.header.ssth);
  if (packet.header.ack) {
    strcat(msg, "ACK ");
  }
  if (packet.header.syn) {
    strcat(msg, "SYN ");
  }
  if (packet.header.fin) {
    strcat(msg, "FIN ");
  }
  if (packet.header.dup) {
    strcat(msg, "DUP ");
  }
  printf("%s\n", msg);
}

void sendSyn(const int sockfd, const struct sockaddr_in clientaddr,
  const unsigned short seqnum, const unsigned short acknum,
  const unsigned short cwnd, const unsigned short ssth) {
  struct Packet syn_packet = packet_new(seqnum, acknum, cwnd, ssth,
    false, true, false, false);
  if (sendto(sockfd, &syn_packet, sizeof(syn_packet), 0,
  (struct sockaddr*) &clientaddr, sizeof(clientaddr)) == -1) {
    perror("ERROR: send");
    exit(1);
  }
  printout_send(syn_packet);
}

void sendSynAck(const int sockfd, const struct sockaddr_in clientaddr,
  const unsigned short seqnum, const unsigned short acknum,
  const unsigned short cwnd, const unsigned short ssth) {
  struct Packet synack_packet = packet_new(seqnum, acknum, cwnd, ssth,
    true, true, false, false);
  if (sendto(sockfd, &synack_packet, sizeof(synack_packet), 0,
  (struct sockaddr*) &clientaddr, sizeof(clientaddr)) == -1) {
    perror("ERROR: send");
    exit(1);
  }
  printout_send(synack_packet);
}

void sendFin(const int sockfd, const struct sockaddr_in clientaddr,
  const unsigned short seqnum, const unsigned short acknum,
  const unsigned short cwnd, const unsigned short ssth) {
  struct Packet fin_packet = packet_new(seqnum, acknum, cwnd, ssth,
    false, false, true, false);
  if (sendto(sockfd, &fin_packet, sizeof(fin_packet), 0,
  (struct sockaddr*) &clientaddr, sizeof(clientaddr)) == -1) {
    perror("ERROR: send");
    exit(1);
  }
  printout_send(fin_packet);
}

void sendFinAck(const int sockfd, const struct sockaddr_in clientaddr,
  const unsigned short seqnum, const unsigned short acknum,
  const unsigned short cwnd, const unsigned short ssth) {
  struct Packet finack_packet = packet_new(seqnum, acknum, cwnd, ssth,
    true, false, true, false);
  if (sendto(sockfd, &finack_packet, sizeof(finack_packet), 0,
  (struct sockaddr*) &clientaddr, sizeof(clientaddr)) == -1) {
    perror("ERROR: send");
    exit(1);
  }
  printout_send(finack_packet);
}

void sendAck(const int sockfd, const struct sockaddr_in clientaddr,
  const unsigned short seqnum, const unsigned short acknum,
  const unsigned short cwnd, const unsigned short ssth) {
  struct Packet ack_packet = packet_new(seqnum, acknum, cwnd, ssth,
    true, false, false, false);
  if (sendto(sockfd, &ack_packet, sizeof(ack_packet), 0,
  (struct sockaddr*) &clientaddr, sizeof(clientaddr)) == -1) {
    perror("ERROR: send");
    exit(1);
  }
  printout_send(ack_packet);
}

void sendPacket(const int sockfd, const struct sockaddr_in clientaddr,
const struct Packet packet) {
  if (sendto(sockfd, &packet, sizeof(packet), 0,
  (struct sockaddr*) &clientaddr, sizeof(clientaddr)) == -1) {
    perror("ERROR: send");
    exit(1);
  }
  printout_send(packet);
}

struct Packet waitForFinPacket(int sockfd) {
  struct Packet recv_packet = packet_empty();
  while(1) {
    fd_set select_fds;
    struct timeval timeout;
    FD_ZERO(&select_fds);
    FD_SET(sockfd, &select_fds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (select(32, &select_fds, NULL, NULL, &timeout) == 0){
      return recv_packet;
    }
    else {
      if (recvfrom(sockfd, &recv_packet, PACKET_SIZE, 0 , NULL, NULL) < 0) {
         perror("ERROR: recvfrom");
         exit(1);
      }
      printout_recv(recv_packet);
      if (recv_packet.header.fin) {
        return recv_packet;
      }
    }
  }
}

struct Packet waitForPacket(int sockfd) {
  struct Packet recv_packet = packet_empty();
  while(1) {
    fd_set select_fds;
    struct timeval timeout;
    FD_ZERO(&select_fds);
    FD_SET(sockfd, &select_fds);
    timeout.tv_sec = CONNECTION_TIMEOUT;
    timeout.tv_usec = 0;
    if (select(32, &select_fds, NULL, NULL, &timeout) == 0){
      return recv_packet;
    }
    else {
      if (recvfrom(sockfd, &recv_packet, PACKET_SIZE, 0 , NULL, NULL) < 0) {
         perror("ERROR: recvfrom");
         exit(1);
      }
      printout_recv(recv_packet);
      return recv_packet;
    }
  }
}

struct Packet waitForPacketData(int sockfd) {
  struct Packet recv_packet = packet_empty();
  while(1) {
    fd_set select_fds;
    struct timeval timeout;
    FD_ZERO(&select_fds);
    FD_SET(sockfd, &select_fds);
    timeout.tv_sec = CONNECTION_TIMEOUT;
    timeout.tv_usec = 0;
    if (select(32, &select_fds, NULL, NULL, &timeout) == 0){
      return recv_packet;
    }
    else {
      if (recvfrom(sockfd, &recv_packet, PACKET_SIZE, 0 , NULL, NULL) < 0) {
         perror("ERROR: recvfrom");
         exit(1);
      }
      printout_recv(recv_packet);
      return recv_packet;
    }
  }
}

struct Packet waitForAckPacket(const int sockfd) {
  struct Packet recv_packet = packet_empty();
  while(1) {
    fd_set select_fds;
    struct timeval timeout;
    FD_ZERO(&select_fds);
    FD_SET(sockfd, &select_fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    if (select(32, &select_fds, NULL, NULL, &timeout) == 0){
      printf("TIMEOUT...\n");
      return recv_packet;
    }
    else {
      if (recvfrom(sockfd, &recv_packet, PACKET_SIZE, 0 , NULL, NULL) < 0) {
         perror("ERROR: recvfrom");
         exit(1);
      }
      printout_recv(recv_packet);
      if (recv_packet.header.fin) {
        return recv_packet;
      }
      if (recv_packet.header.ack) {
        return recv_packet;
      }
    }
  }
}

bool closeConnection(const int sockfd, const struct sockaddr_in clientaddr,
  const unsigned short seqnum, const unsigned short acknum,
  const unsigned short cwnd, const unsigned short ssth) {
  // Send FIN packet
  sendFin(sockfd, clientaddr, seqnum, acknum, cwnd, ssth);
  // Expect ACK packet
  struct Packet recv_packet = waitForPacket(sockfd);
  if (!recv_packet.header.ack) { // Not ACK packet
    return false;
  }
  // Expect FIN packet within 2s
  recv_packet = waitForFinPacket(sockfd);
  if (!recv_packet.header.fin) {
    return false;
  }

  // Respond with ACK packet
  sendAck(sockfd, clientaddr, seqnum+1, recv_packet.header.seqnum + 1,
    recv_packet.header.cwnd, recv_packet.header.ssth);
  return true;
}

bool respondToFin(const int sockfd, const struct sockaddr_in clientaddr,
  const unsigned short seqnum, const unsigned short acknum,
  const unsigned short cwnd, const unsigned short ssth) {
  // Send ACK packet
  sendAck(sockfd, clientaddr, seqnum, acknum, cwnd, ssth);
  // Send Fin packet
  sendFin(sockfd, clientaddr, seqnum, 0, cwnd, ssth);
  // Expect ACK
  struct Packet recv_packet = waitForPacket(sockfd);
  if (!recv_packet.header.ack) {
    return false;
  }
  return true;
}

/*
Function to verify the working client/server
*/
bool validClient(const struct sockaddr_in client1, const struct sockaddr_in client2) {
if (client1.sin_family != client2.sin_family) {
  return false;
}

if (client1.sin_port != client2.sin_port) {
  return false;
}

if (client1.sin_addr.s_addr != client2.sin_addr.s_addr) {
  return false;
}

return true;
}
