
#include <stdio.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "msocket.h"

#define P(s) semop(s, &sem->pop, 1)
#define V(s) semop(s, &sem->vop, 1)

// Helper functions
int min(int a, int b)
{
    return a > b ? b : a;
}

int cpy1(char s1[], char s2[], int l)
{
    for (int i = 0; i < l; i++)
        s1[i] = s2[i];
}

// Socket Functions
int m_socket(int family, int type, int flag)
{
    // Not of MTP
    if (type != SOCK_MTP)
        return -1;

    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);

    key_t fkey6 = ftok(".", 6);
    int SHM_id2 = shmget(fkey6, sizeof(sock_info), 0666);
    sock_info *si = (sock_info *)shmat(SHM_id2, (void *)0, 0);
    P(sem->semid3);
    int i;
    for (i = 0; i < N; i++)
    {
        if (SHM[i].free == 1)
        {
            SHM[i].free = 0;
            break;
        }
    }
    if (i == N)
    {
        errno = ENOBUFS;
        V(sem->semid3);
        shmdt(SHM);
        shmdt(sem);
        shmdt(si);
        return -1;
    }

    si->port = 0;
    si->sockfd = 0;
    strcpy(si->ip, "");
    si->err_no = 0;

    V(sem->semid1);
    P(sem->semid2);
    errno = si->err_no;

    if (si->sockfd == -1)
    {
        perror("Error occured in socket creation:");
        SHM[i].free = 1;
        V(sem->semid3);
        shmdt(SHM);
        shmdt(sem);
        shmdt(si);
        return -1;
    }
    SHM[i].sockfd = si->sockfd;
    SHM[i].pid = getpid();
    SHM[i].free_sndbuf = 10;
    SHM[i].free_rcvbuf = 5;
    SHM[i].first_snd = 0;
    SHM[i].last_snd = 0;
    SHM[i].first_recv = 0;
    SHM[i].last_recv = 0;
    SHM[i].ack_expected = -1;
    SHM[i].selfport = 0;
    SHM[i].next_frame = 1;
    SHM[i].dest_rwnd_size = 5;
    SHM[i].size_recv = 5;
    SHM[i].next_recv = -1;
    SHM[i].last_in_order = 15;
    SHM[i].last_ack_recv = 0;
    for (int j = 0; j < 5; j++)
    {
        SHM[i].next_recv++;
        SHM[i].next_recv %= 5;
        SHM[i].rwnd[j] = j + 1;
    }
    SHM[i].last_expected = 5;

    V(sem->semid3);

    shmdt(SHM);
    shmdt(sem);
    shmdt(si);
    return i;
}

int m_bind(int mtp_sockfd, char selfip[], int selfport, char destip[], int destport)
{
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);
    sock_mem s = SHM[mtp_sockfd];

    key_t fkey6 = ftok(".", 6);
    int SHM_id2 = shmget(fkey6, sizeof(sock_info), 0666);
    sock_info *si = (sock_info *)shmat(SHM_id2, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);

    si->port = selfport;
    si->sockfd = s.sockfd;
    strcpy(si->ip, selfip);

    si->err_no = 0;
    P(sem->semid3);
    V(sem->semid1);
    P(sem->semid2);
    errno = si->err_no;

    if (si->sockfd == -1)
    {
        SHM[mtp_sockfd].free = 1;
        perror("Error occured in socket binding:");
        SHM[mtp_sockfd].free = 1;
        V(sem->semid3);
        shmdt(SHM);
        shmdt(sem);
        shmdt(si);
        return -1;
    }
    strcpy(SHM[mtp_sockfd].destip, destip);
    strcpy(SHM[mtp_sockfd].selfip, selfip);
    SHM[mtp_sockfd].destport = destport; // port stored in host ordering itself
    SHM[mtp_sockfd].selfport = selfport; // port stored in host ordering itself
    V(sem->semid3);

    shmdt(SHM);
    shmdt(sem);
    shmdt(si);

    return 0; // success
}

int m_sendto(int mtp_socket, char message[], int len_message, int flag, char destip[], int destport)
{
    // len_message = min(len_message, (int)sizeof(message)/sizeof(char));
    len_message = min(len_message, 1024);
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);
    P(sem->semid3);
    int j = 0;
    for (j = 0; j < 25; j++)
    {
        if (SHM[j].free == 0 && SHM[j].selfport == destport && SHM[j].destport == SHM[mtp_socket].selfport)
        {
            break;
        }
    }
    // j will come out with 25 if no socket is bound to dest port or bound and dest of dest is not self.

    if (strcmp(SHM[mtp_socket].destip, destip) != 0 || SHM[mtp_socket].destport != destport || j == 25)
    {
        errno = ENOTCONN;
        V(sem->semid3);

        shmdt(SHM);
        shmdt(sem);
        return -1;
    }
    else if (SHM[mtp_socket].free_sndbuf == 0)
    {
        errno = ENOBUFS;
        V(sem->semid3);
        shmdt(SHM);
        shmdt(sem);

        return -1;
    }
    else
    {
        char locmessage[1050];
        cpy1(locmessage, message, len_message);
        char sframe[3];
        int num = (SHM[mtp_socket].next_frame);
        SHM[mtp_socket].next_frame %= 15;
        SHM[mtp_socket].next_frame += 1;
        sprintf(sframe, "%02d", num);
        // strcat(locmessage, sframe);
        for (int i = 0; i < 2; i++)
        {

            locmessage[i + len_message] = sframe[i];
            // printf("loc %s\n", locmessage);
        }
        len_message += 2;
        // strcat(locmessage, "0"); // last 0 indicates sending data message
        locmessage[len_message] = '0';
        len_message++;
        cpy1(SHM[mtp_socket].send_buffer[SHM[mtp_socket].last_snd], locmessage, len_message);
        SHM[mtp_socket].send_len[SHM[mtp_socket].last_snd] = len_message;
        SHM[mtp_socket].free_sndbuf--;
        SHM[mtp_socket].last_snd = (SHM[mtp_socket].last_snd + 1) % 10;
        V(sem->semid3);
        shmdt(SHM);
        shmdt(sem);

        return len_message - 3;
    }
}

int m_recvfrom(int mtp_socket, char message[], int len_message, int flag, char fromip[], int fromport)
{
    // TODO: Check ip port
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666 | IPC_CREAT);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);
    P(sem->semid3);
    int f = SHM[mtp_socket].free_rcvbuf;

    if (f == 5)
    {
        // all spaces in buffer are free
        errno = ENOMSG;
        V(sem->semid3);

        shmdt(SHM);
        shmdt(sem);
        return -1;
    }
    else
    {
        // len_message=min(sizeof(message),len_message);
        len_message = min(1024, len_message);
        len_message= min(SHM[mtp_socket].recieve_len[SHM[mtp_socket].first_recv],len_message);
        cpy1(message, SHM[mtp_socket].recieve_buffer[SHM[mtp_socket].first_recv],len_message);
        
        SHM[mtp_socket].free_rcvbuf++;
        SHM[mtp_socket].first_recv = (SHM[mtp_socket].first_recv + 1) % 5;
        SHM[mtp_socket].last_expected %= 15;
        SHM[mtp_socket].last_expected += 1;
        SHM[mtp_socket].next_recv++;
        SHM[mtp_socket].next_recv %= 5;
        SHM[mtp_socket].size_recv += 1;
        SHM[mtp_socket].rwnd[SHM[mtp_socket].next_recv] = SHM[mtp_socket].last_expected;
        V(sem->semid3);

        shmdt(SHM);
        shmdt(sem);
        return len_message;
    }
}

int m_close(int mtp_socket)
{
    // printf("Entering close\n");
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);
    P(sem->semid3);
    if (SHM[mtp_socket].free ||
        SHM[mtp_socket].free_sndbuf < 10) // there is still something left to send in send buffer.
    {
        V(sem->semid3);
        // printf("Close exiting1\n");
        shmdt(sem);
        return -1;
    }
    else
    {
        key_t fkey6 = ftok(".", 6);
        int SHM_id2 = shmget(fkey6, sizeof(sock_info), 0666);
        sock_info *si = (sock_info *)shmat(SHM_id2, (void *)0, 0);

        si->sockfd = SHM[mtp_socket].sockfd;
        si->err_no = 0;
        strcpy(si->ip, "");
        si->port = 0;
        si->msockfd = mtp_socket;

        V(sem->semid1);
        P(sem->semid2);
        SHM[mtp_socket].free = 1;
        SHM[mtp_socket].sockfd = -1;
        SHM[mtp_socket].selfport = 0;
        SHM[mtp_socket].destport = 0;
        SHM[mtp_socket].frame_expected = 0;
        strcpy(SHM[mtp_socket].selfip, "");
        strcpy(SHM[mtp_socket].destip, "");
        errno = si->err_no;

        V(sem->semid3);
        shmdt(sem);
        shmdt(si);
        // printf("Close exiting2\n");
        return 0;
    }
    V(sem->semid3);

    shmdt(SHM);
    shmdt(sem);
    // printf("Close exiting3\n");
    return 0;
}


