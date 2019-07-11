/*
Contains the structure of packet
*/
#include <string.h>
#include <stdbool.h>
#include "constant.h"

struct Header {
  unsigned short seqnum;
  unsigned short acknum;
  bool ack;
  bool syn;
  bool fin;
  bool dup;
  unsigned short cwnd;
  unsigned short ssth;
};

struct Packet {
    struct Header header; //12 bytes
    char data[MAX_DATA_SIZE]; // 510 bytes
    unsigned short data_size; //2 bytes
};

struct Packet packet_empty() {
  struct Packet packet;
  packet.header.seqnum = 0;
  packet.header.seqnum = 0;
  packet.header.ack = false;
  packet.header.syn = false;
  packet.header.fin = false;
  packet.header.dup = false;
  packet.header.cwnd = 0;
  packet.header.ssth = 0;
  memset(packet.data, '\0', sizeof(packet.data));
  packet.data_size = 0;
  return packet;
}
/*Create a new packet with no data*/
struct Packet packet_new(
  unsigned short seqnum, unsigned short acknum,
  unsigned short cwnd, unsigned short ssth,
  bool ack, bool syn, bool fin, bool dup) {
  struct Packet packet;
  packet.header.seqnum = seqnum;
  packet.header.acknum = acknum;
  packet.header.ack = ack;
  packet.header.syn = syn;
  packet.header.fin = fin;
  packet.header.dup = dup;
  packet.header.cwnd = cwnd;
  packet.header.ssth = ssth;
  memset(packet.data, '\0', sizeof(packet.data));
  packet.data_size = 0;
  return packet;
};

/*Create a new packet with data*/
struct Packet packet_data_new(
  unsigned short seqnum, unsigned short acknum,
  unsigned short cwnd, unsigned short ssth,
  bool ack, bool syn, bool fin, bool dup,
  char* data, unsigned short data_size) {
  struct Packet packet;
  packet.header.seqnum = seqnum;
  packet.header.acknum = acknum;
  packet.header.ack = ack;
  packet.header.syn = syn;
  packet.header.fin = fin;
  packet.header.dup = dup;
  packet.header.cwnd = cwnd;
  packet.header.ssth = ssth;
  memcpy(packet.data,data,data_size);
//  strcpy(packet.data, data);
  packet.data_size = data_size;
  return packet;
};
