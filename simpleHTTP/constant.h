/*
Contains all constants used in this project
*/

#define HEADER_SIZE 12
#define PACKET_SIZE 524
#define MAX_DATA_SIZE 510 //2 bytes for data_size
#define MAX_SEQ_NUM 25600
#define RTO 0.5 //sec
#define CONNECTION_TIMEOUT 10 //sec
#define MAX_CWND 10240
#define MIN_CWND 512
#define INITIAL_SSTH 5120
#define MAX_FILE_SIZE 100000000 //byte
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
