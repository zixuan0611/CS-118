// Client side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>

#include "constant.h"
#include "protocol.h"

int sockfd;
struct sockaddr_in servaddr;

unsigned short tcp_state; //SLOW_START, CONGESTION_AVOIDANCE
unsigned short init_seqnum;
unsigned short seqnum;
unsigned short acknum;
unsigned short cwnd;
unsigned short ssth;

FILE* filefd = NULL;

//long int file_size;
//long int factor_seqnum;
//long int base_seqnum;
unsigned long file_size;
unsigned long factor_seqnum;
unsigned long base_seqnum;
// long int next_seqnum;
unsigned short dupACKcount;
struct Packet data_packet;
clock_t timer;
clock_t timer_connection;
pthread_mutex_t base_lock;
pthread_mutex_t next_seqnum_lock;
char buffer[MAX_DATA_SIZE];


bool threeWayHandshake(const int sockfd, const struct sockaddr_in servaddr,
                       int file_size){
  // Start first SYN
  sendSyn(sockfd, servaddr, seqnum, acknum, cwnd, ssth);

  // Expect SYN-ACK packet
  struct Packet recv_packet = waitForPacket(sockfd);
  if (!recv_packet.header.syn || !recv_packet.header.ack) {
    close(sockfd);
    return false;
  }
  seqnum = recv_packet.header.acknum;
  acknum = recv_packet.header.seqnum + 1;

  // Send final ACK
  if (file_size==0){
    sendAck(sockfd, servaddr, seqnum, acknum, cwnd, ssth);
  }
  return true;
}

long int getFileSize(FILE* filefd){
  fseek(filefd, 0, SEEK_END); // move pointer to the end of the file
  unsigned long file_size = ftell(filefd);
  fseek(filefd, 0, SEEK_SET); // move pointer back to the front of the file
  return file_size;
}


void* sendPacketThread(void *filefd){
  filefd = (FILE *) filefd;
//  FILE* filefd_client = fopen("test", "wb");
  while (1){
    //  Read and send whenever seqnum < min(file_size,base + cwnd)
//    long int file_curr_index = factor_seqnum+seqnum-init_seqnum-1;
    unsigned long file_curr_index = factor_seqnum+seqnum-init_seqnum-1;
    if (file_curr_index>=file_size){
      break;
    }
    if (factor_seqnum+seqnum <  base_seqnum + cwnd){
      fseek(filefd,file_curr_index,SEEK_SET);
      memset(buffer, '\0', sizeof(buffer));
      fread(&buffer, sizeof(char), MAX_DATA_SIZE, filefd);
      unsigned short data_size = MIN(MAX_DATA_SIZE,file_size-file_curr_index);

      if (data_size > 0) {
        bool ackFlag = false;
        if (acknum != 0) {
          ackFlag = true;
        }
        data_packet = packet_data_new(seqnum, acknum, cwnd, ssth,
                                      ackFlag, false, false, false, buffer,data_size);
        sendPacket(sockfd, servaddr, data_packet);
        seqnum += data_size;
        if (seqnum>MAX_SEQ_NUM){
          factor_seqnum += seqnum;
          seqnum = 0;
        }
//        fwrite(buffer, sizeof(char), data_size, filefd_client);
      }
    }
    //sleep(0.5);
    //usleep(1000);
  }
//  fclose(filefd_client);
//  filefd_client = NULL;
  return NULL;
}

int main(int argc, char** argv) {
  if (argc != 4) {
    fprintf(stderr, "%s\n", "ERROR: ./client <hostname-or-ip> <port> <filename>");
    exit(1);
  }
  const unsigned int PORT = strtol(argv[2], NULL, 10);
  const char* filename = argv[3];

  struct hostent *he;
  if ((he = gethostbyname(argv[1])) == NULL) {
    perror("ERROR: invalid hostname");
    exit(1);
  }

	// Creating socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("ERROR: socket");
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));

	// Filling server information
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
  memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);

  //============Init tcp congestion control==============
  tcp_state = SLOW_START;
  cwnd = MIN_CWND;
  ssth = INITIAL_SSTH;
  srand(time(0));
  init_seqnum = rand() % 5000;
  seqnum = init_seqnum;
  base_seqnum = init_seqnum;
  factor_seqnum = 0;
  // next_seqnum = base_seqnum + cwnd;
  acknum = 0;

  pthread_t tid;
  int send_packet_thread;
  // Read file
  filefd = fopen(filename, "rb");
  file_size = getFileSize(filefd);

  //==========Perform three-way handshaking==========
  if (threeWayHandshake(sockfd, servaddr, file_size)==false){
    fprintf(stderr, "%s\n", "ERROR: Failed to perform handshaking");
    return 0;
  }

  //==========Performing data transmission=========
  if (file_size > 0){
    struct Packet recv_packet;
    send_packet_thread = pthread_create(&tid, NULL, sendPacketThread, filefd);
    timer = clock();
	  timer_connection = clock();

    while (1){
      if (recvfrom(sockfd, &recv_packet, PACKET_SIZE, 0 , NULL, NULL) < 0){
         perror("ERROR: recvfrom");
         exit(1);
      }
      //-----------Handling received ack event---------
      if (recv_packet.header.ack) {
         printout_recv(recv_packet);
         // Update base_seqnum as the cummulative ack
         if (recv_packet.header.acknum > seqnum){
        	 base_seqnum = factor_seqnum + recv_packet.header.acknum - MAX_SEQ_NUM;
         }
         else{
        	 base_seqnum = factor_seqnum + recv_packet.header.acknum;
         }

         acknum = 0; //recv_packet.header.seqnum + 1;

         // SLOW_START
         if (tcp_state == SLOW_START){
           if (cwnd < ssth){
             cwnd = MIN(cwnd+MAX_DATA_SIZE,MAX_CWND);
           }
           else{
              tcp_state = CONGESTION_AVOIDANCE;
           }
         }
         // CONGESTION_AVOIDANCE
         else if (tcp_state == CONGESTION_AVOIDANCE){
           cwnd = MIN(cwnd+(MAX_DATA_SIZE*MAX_DATA_SIZE)/cwnd, MAX_CWND);
         }
         // UNDEFINED STATE
         else {
           exit(1);
         }
         // Check if all segments in the file are acknowledge
        // printf("---%ld/%ld\n",factor_seqnum+seqnum-init_seqnum-1,file_size);
         if (factor_seqnum+seqnum-init_seqnum-1>=file_size){
           break;
         }
         else{ // Restart timer
           timer = clock();
         }
         // Reset connection timeout whenever there is incomming packet
		     timer_connection = clock();
       }
       //-------------Handling Timeout event------------
      double elapse_time = (double) (clock() - timer) / CLOCKS_PER_SEC;
      if (elapse_time > RTO){
         // Reduce ssthresh
         ssth = MAX(cwnd/2.0,1024);

         // Reset cwnd
         cwnd = MIN_CWND;

         // Retransmit not-yet-acked segment with the smallest seqnum
         fseek(filefd,factor_seqnum+base_seqnum-init_seqnum-1,SEEK_SET);
         memset(buffer, '\0', sizeof(buffer));
         fread(&buffer, sizeof(char), MAX_DATA_SIZE, filefd);
         data_packet = packet_data_new(base_seqnum-factor_seqnum-init_seqnum, acknum, cwnd, ssth,
                                         false, false, false, false, buffer, sizeof(buffer));
         sendPacket(sockfd, servaddr, data_packet);

         // Change to SLOW_START tcp_state after retransmission
         tcp_state = SLOW_START;

         // Restart timer
         timer = clock();
       }

      //------------Handling connection timeout event--------
    	elapse_time = (double) (clock() - timer_connection) / CLOCKS_PER_SEC;
    	if (elapse_time > CONNECTION_TIMEOUT){
    		exit(1);
    	}
    }
    //=========Stop file data sending packet thread=======
    if (send_packet_thread==0){
      pthread_join(tid,NULL);
    }
  }

  //=========Perform closing connection===========
  closeConnection(sockfd, servaddr, seqnum, acknum, cwnd, ssth);

  close(sockfd);
  return 0;
}
