
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

// Macros
#define N 25
#define SOCK_MTP 3
#define T 5
// Probability for drop message
#define PR (float)0.4

// Struct for a socket
typedef struct
{
    int free; // boolean for whether or not this socket is free or not
    int pid;
    int sockfd;

    char selfip[20];
    int selfport;
    char destip[20];
    int destport;

    char send_buffer[10][1050];
    int send_len[10];
    int free_sndbuf;
    int last_snd, first_snd;

    char recieve_buffer[5][1050];
    int recieve_len[5];
    int nospace;
    int free_rcvbuf;
    int last_recv, first_recv;

    int ack_expected;
    int frame_expected;

    int time_wait;
    int next_frame;//to be allocated
    int dest_rwnd_size;
    int first_frame_sent;//this is the first frame number of the series of frames sent latest. 
    int last_ack_recv;

    int last_in_order;
    int rwnd[5];
    int last_expected;
    char temp_buf[5][1500];
    int temp_len[5];
    int next_recv;
    int size_recv;
    int ack_time;//last time ack was sent
} sock_mem;
// Struct for semaphores
typedef struct
{
    int semid1;
    int semid2;
    int semid3;
    struct sembuf pop, vop;

} SEMS;
// Struct for sock_info
typedef struct
{
    int sockfd;
    int msockfd;
    char ip[20]; // src ip
    int port;
    int err_no;
} sock_info;

int m_socket(int family, int type, int flag);
int m_bind(int mtp_sockfd, char selfip[], int selfport, char destip[], int destport);
int m_sendto(int mtp_socket, char message[], int len_message, int flag, char destip[], int destport);
int m_recvfrom(int mtp_socket, char message[], int len_message, int flag, char fromip[], int fromport);
int m_close(int mtp_socket);
