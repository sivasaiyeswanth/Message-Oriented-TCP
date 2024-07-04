
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

pthread_mutex_t mutex;
int flag = 0;

#define P(s) semop(s, &sem->pop, 1)
#define V(s) semop(s, &sem->vop, 1)

// Signal (Ctrl-C) handler
void sighandler()
{
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);

    key_t fkey1 = ftok(".", 2);
    key_t fkey2 = ftok(".", 3);
    key_t fkey3 = ftok(".", 4);

    sem->semid1 = semget(fkey1, 1, 0777);
    sem->semid2 = semget(fkey2, 1, 0777);
    sem->semid3 = semget(fkey3, 1, 0777);

    key_t fkey6 = ftok(".", 6);
    int SHM_id2 = shmget(fkey6, sizeof(sock_info), 0666);
    sock_info *si = (sock_info *)shmat(SHM_id2, (void *)0, 0);

    semctl(sem->semid1, 0, IPC_RMID, 0);
    semctl(sem->semid2, 0, IPC_RMID, 0);
    semctl(sem->semid3, 0, IPC_RMID, 0);
    shmdt(SHM);
    shmdt(sem);
    shmdt(si);
    shmctl(SHM_id, IPC_RMID, NULL);
    shmctl(SHM_id1, IPC_RMID, NULL);
    shmctl(SHM_id2, IPC_RMID, NULL);
    pthread_mutex_destroy(&mutex);

    printf("Exiting...\n");
    exit(0);
}

int drop_message()
{

    // Generate a random number between 0 and 99
    int randomNumber = rand() % 100;
    float f = randomNumber / 100.0;
    return f < PR;
}

int cpy2(char s1[], char s2[], int l)
{
    for (int i = 0; i < l; i++)
        s1[i] = s2[i];
}

int max(int a, int b)
{
    return a > b ? a : b;
}

// R thread
void *R_main()
{
    char buffer[2000];
    printf("Thread R created\n");

    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666 | IPC_CREAT);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);
    while (1)
    {
        pthread_mutex_lock(&mutex);
        P(sem->semid3);
        if (flag == 1)
        {
            V(sem->semid3);
            pthread_mutex_unlock(&mutex);
            break;
        }
        int maxfd = 0;
        fd_set readfd;
        struct timeval timeout;
        timeout.tv_sec = T;
        timeout.tv_usec = 0;
        for (int i = 0; i < N; i++)
        {
            if (SHM[i].free == 0 && SHM[i].selfport != 0)
            {
                // Resetting if space found free
                if (SHM[i].size_recv && SHM[i].nospace == 1)
                {

                    SHM[i].nospace = 0;
                }
                // If space is free add it in select call
                if (SHM[i].nospace == 0)
                {
                    maxfd = max(maxfd, SHM[i].sockfd);
                    FD_SET(SHM[i].sockfd, &readfd);
                }
            }
        }
        if (maxfd == 0)
        {
            V(sem->semid3);
            pthread_mutex_unlock(&mutex);
            continue;
        }
        V(sem->semid3);
        pthread_mutex_unlock(&mutex);
        select(maxfd + 1, &readfd, NULL, NULL, &timeout);

        pthread_mutex_lock(&mutex);
        P(sem->semid3);
        for (int i = 0; i < N; i++)
        {
            time_t current_time = time(NULL);
            // Sending Ack if space in recieve
            if (SHM[i].sockfd != -1 && SHM[i].free == 0 && !FD_ISSET(SHM[i].sockfd, &readfd) && SHM[i].nospace == 0 && current_time - SHM[i].ack_time >= T)
            {
                int loc = (SHM[i].last_in_order % 15) + 1;

                struct sockaddr_in cli_addr;
                cli_addr.sin_family = AF_INET;
                cli_addr.sin_port = htons(SHM[i].destport);
                inet_aton(SHM[i].destip, &cli_addr.sin_addr);

                char ack[100];
                sprintf(ack, "ACK%d001", SHM[i].size_recv);
                sendto(SHM[i].sockfd, ack, strlen(ack), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                SHM[i].ack_time = current_time;
            }
            if (SHM[i].sockfd != -1 && SHM[i].free == 0 && FD_ISSET(SHM[i].sockfd, &readfd))
            {

                struct sockaddr_in cliaddr;
                int lenaddr = sizeof(cliaddr);
                int len = recvfrom(SHM[i].sockfd, buffer, 1050, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &lenaddr);
                buffer[len] = '\0';
                if (len == -1)
                {
                    continue;
                }
                if (drop_message() == 1)
                {
                    printf("Dropped the message: ");
                    for (int i = 0; i < 10; i++)
                    {
                        if (i >= len)
                            break;
                        printf("%c", buffer[i]);
                    }
                    printf("... on socket %d\n", i);
                    continue;
                }
                // Case: ACK Message(Format --> "ACK(binary:Acknum)(binary:Identifier)\0")
                // let's assume acknowledgment sent is of form ACKRXY1 where R is rwnd size
                if (buffer[len - 1] == '1')
                {
                    SHM[i].dest_rwnd_size = buffer[3] - '0';

                    int ack = 10 * (buffer[4] - '0');
                    ack += buffer[5] - '0';
                    if (ack != 0)
                    {
                        // this is the last frame received properly from the series of frames sent
                        ack %= 15;
                        ack++;
                        // SHM[i].last_ack_recv = ack;
                        int how_many = 0;
                        if (ack >= SHM[i].first_frame_sent)
                        {
                            // suppose 1,2,3,4,5 have been sent and only 1,4,5 have been sent so ACk2 will be sent and thus we have exactly 1 inorder message recevied
                            how_many = ack - SHM[i].first_frame_sent;
                            // printf("F\n");
                        }
                        else
                        {
                            // suppose we have sent 14,15,1,2,3 and we received ACK2==>14,15,1 were received ack is 2,first_frame_sent is 14
                            how_many = 16 - SHM[i].first_frame_sent;
                            how_many += ack - 1;
                            // printf("S\n");
                        }
                        SHM[i].first_frame_sent += how_many;
                        SHM[i].first_frame_sent %= 15;
                        if (!SHM[i].first_frame_sent)
                            SHM[i].first_frame_sent = 15;
                        SHM[i].first_snd = (SHM[i].first_snd + how_many) % 10;
                        // printf("How many:%d and first_frame:%d\n", how_many, SHM[i].first_frame_sent);
                        SHM[i].free_sndbuf += how_many;
                    }
                }
                else
                {
                    int frame_recv = 0;
                    frame_recv = 10 * (buffer[len - 3] - '0') + (buffer[len - 2] - '0');
                    int j = 0;
                    for (j = 0; j < 5; j++)
                    {
                        if (SHM[i].rwnd[j] == frame_recv)
                        {
                            cpy2(SHM[i].temp_buf[j], buffer, len - 3);
                            SHM[i].temp_len[j] = len - 3;
                            SHM[i].rwnd[j] = 0;
                            break;
                        }
                    }
                    while (SHM[i].size_recv)
                    {
                        int loc_next_recv = (SHM[i].last_in_order % 15) + 1;

                        int j = (loc_next_recv - 1) % 5;
                        if (SHM[i].rwnd[j] == 0)
                        {
                            SHM[i].size_recv--;
                            cpy2(SHM[i].recieve_buffer[SHM[i].last_recv], SHM[i].temp_buf[j], SHM[i].temp_len[j]);
                            SHM[i].recieve_len[SHM[i].last_recv] = SHM[i].temp_len[j];
                            SHM[i].last_in_order %= 15;
                            SHM[i].last_in_order += 1;
                            SHM[i].last_recv++;
                            SHM[i].last_recv %= 5;
                            SHM[i].free_rcvbuf--;
                        }
                        else
                        {
                            break;
                        }
                    }

                    // Send ack;
                    int loc = (SHM[i].last_in_order);
                    if (SHM[i].size_recv == 0)
                        SHM[i].nospace = 1;
                    char ack[100];
                    sprintf(ack, "ACK%d%02d1", SHM[i].size_recv, loc);
                    sendto(SHM[i].sockfd, ack, strlen(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                    SHM[i].ack_time = current_time;
                    printf("Sent ack: %s from socket %d\n", ack, i);
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        V(sem->semid3);
    }
}
// S thread
void *S_main()
{
    printf("Thread S created\n");

    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666 | IPC_CREAT);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);

    while (1)
    {
        pthread_mutex_lock(&mutex);
        P(sem->semid3);
        if (flag == 1)
        {
            V(sem->semid3);
            pthread_mutex_unlock(&mutex);
            break;
        }

        for (int i = 0; i < N; i++)
        {
            if (SHM[i].free == 0 && SHM[i].free_sndbuf != 10 && SHM[i].selfport != 0)
            {
                // Send the frame if either ack for previous message recieved or timed out
                if (SHM[i].time_wait <= 0)
                {
                    // Client address
                    struct sockaddr_in cliaddr;
                    cliaddr.sin_family = AF_INET;
                    cliaddr.sin_port = htons(SHM[i].destport);
                    inet_aton(SHM[i].destip, &cliaddr.sin_addr);
                    // sent 0 frames for this socket in this iteration
                    int sent = 0;
                    // atmost these many can be sent
                    int max_send = 10 - SHM[i].free_sndbuf;
                    while (sent < max_send && sent < SHM[i].dest_rwnd_size)
                    {
                        if (sent == 0)
                        {
                            // extracting the first frame sent in the sequence
                            int len = SHM[i].send_len[SHM[i].first_snd];
                            char buf1[2015];
                            cpy2(buf1, SHM[i].send_buffer[SHM[i].first_snd], len);
                            SHM[i].first_frame_sent = 10 * (buf1[len - 3] - '0');
                            SHM[i].first_frame_sent += (buf1[len - 2] - '0');
                        }
                        int next_snd = SHM[i].first_snd + sent;
                        next_snd %= 10;
                        char buf1[2015];
                        cpy2(buf1, SHM[i].send_buffer[next_snd], SHM[i].send_len[next_snd]);
                        int n = sendto(SHM[i].sockfd, buf1, SHM[i].send_len[next_snd], 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                        if (n != -1)
                        {
                            printf("Sent: ");

                            for (int j = 0; j < SHM[i].send_len[next_snd]; j++)
                            {
                                if (j == 6)
                                    break;
                                printf("%c", buf1[j]);
                            }
                            printf("... from socket %d\n", i);
                            sent++;
                            SHM[i].time_wait = T;
                        }
                    }
                }
                else
                {
                    SHM[i].time_wait -= (T / 2);
                }
            }
        }
        V(sem->semid3);
        pthread_mutex_unlock(&mutex);
        sleep(T / 2);
    }
}
// Garbage collector thread
void *g_main()
{
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);

    key_t fkey1 = ftok(".", 2);
    key_t fkey2 = ftok(".", 3);
    key_t fkey3 = ftok(".", 4);

    sem->semid1 = semget(fkey1, 1, 0777);
    sem->semid2 = semget(fkey2, 1, 0777);
    sem->semid3 = semget(fkey3, 1, 0777);

    key_t fkey6 = ftok(".", 6);
    int SHM_id2 = shmget(fkey6, sizeof(sock_info), 0666);
    sock_info *si = (sock_info *)shmat(SHM_id2, (void *)0, 0);

    while (1)
    {
        sleep(5 * T);
        pthread_mutex_lock(&mutex);
        P(sem->semid3);
        for (int i = 0; i < N; i++)
        {
            if (SHM[i].free == 0 && kill(SHM[i].pid, 0) == -1 && SHM[i].free_sndbuf == 10)
            {
                close(SHM[i].sockfd);
                SHM[i].free = 1;
                SHM[i].free = 1;
                SHM[i].sockfd = -1;
                SHM[i].selfport = 0;
                SHM[i].destport = 0;
                SHM[i].frame_expected = 0;
                strcpy(SHM[i].selfip, "");
                strcpy(SHM[i].destip, "");
                printf("Cleaned Socket %d\n", i);
                // printf("Pr=%f num_sent=%d\n",PR,num_sent);
                // PR+=0.05;
            }
        }
        pthread_mutex_unlock(&mutex);
        V(sem->semid3);
    }
}

// Thread for exiting on keyboard interrupt
void *Exit_main()
{
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);

    key_t fkey1 = ftok(".", 2);
    key_t fkey2 = ftok(".", 3);
    key_t fkey3 = ftok(".", 4);

    sem->semid1 = semget(fkey1, 1, 0777);
    sem->semid2 = semget(fkey2, 1, 0777);
    sem->semid3 = semget(fkey3, 1, 0777);

    key_t fkey6 = ftok(".", 6);
    int SHM_id2 = shmget(fkey6, sizeof(sock_info), 0666);
    sock_info *si = (sock_info *)shmat(SHM_id2, (void *)0, 0);
    int a = 1;
    while (a)
    {
        printf("Enter 0 to exit\n");
        scanf("%d", &a);
        if (a == 0)
            flag = 1;
    }
    semctl(sem->semid1, 0, IPC_RMID, 0);
    semctl(sem->semid2, 0, IPC_RMID, 0);
    semctl(sem->semid3, 0, IPC_RMID, 0);
    shmdt(SHM);
    shmdt(sem);
    shmdt(si);
    shmctl(SHM_id, IPC_RMID, NULL);
    shmctl(SHM_id1, IPC_RMID, NULL);
    shmctl(SHM_id2, IPC_RMID, NULL);
    pthread_mutex_destroy(&mutex);

    printf("Exiting...\n");
    exit(0);
}

int main()
{
    int seed = 42;
    srand(seed);
    pthread_t r, s, e, g;
    signal(SIGINT, sighandler);
    key_t fkey = ftok(".", 1);
    int SHM_id = shmget(fkey, N * sizeof(sock_mem), 0666 | IPC_CREAT);
    sock_mem *SHM = (sock_mem *)shmat(SHM_id, (void *)0, 0);

    key_t fkey5 = ftok(".", 5);
    int SHM_id1 = shmget(fkey5, sizeof(SEMS), 0666 | IPC_CREAT);
    SEMS *sem = (SEMS *)shmat(SHM_id1, (void *)0, 0);

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_trylock(&mutex);
    pthread_mutex_unlock(&mutex);

    key_t fkey1 = ftok(".", 2);
    key_t fkey2 = ftok(".", 3);
    key_t fkey3 = ftok(".", 4);

    sem->semid1 = semget(fkey1, 1, 0777 | IPC_CREAT);
    sem->semid2 = semget(fkey2, 1, 0777 | IPC_CREAT);
    sem->semid3 = semget(fkey3, 1, 0777 | IPC_CREAT);

    semctl(sem->semid1, 0, SETVAL, 0);
    semctl(sem->semid2, 0, SETVAL, 0);
    semctl(sem->semid3, 0, SETVAL, 1);
    sem->pop.sem_num = sem->vop.sem_num = 0;
    sem->pop.sem_flg = sem->vop.sem_flg = 0;
    sem->pop.sem_op = -1;
    sem->vop.sem_op = 1;

    key_t fkey6 = ftok(".", 6);
    int SHM_id2 = shmget(fkey6, sizeof(sock_info), 0666 | IPC_CREAT);
    sock_info *si = (sock_info *)shmat(SHM_id2, (void *)0, 0);

    for (int i = 0; i < N; i++)
    {
        SHM[i].free = 1;
        SHM[i].sockfd = -1;
        SHM[i].nospace = 0;
        SHM[i].frame_expected = 0;
        SHM[i].ack_expected = -1;
        SHM[i].selfport = 0;
    }
    printf("sock_mem Memory created and initialized successfully\n");
    pthread_create(&r, NULL, R_main, NULL);
    pthread_create(&s, NULL, S_main, NULL);
    pthread_create(&e, NULL, Exit_main, NULL);
    pthread_create(&g, NULL, g_main, NULL);

    int fd;
    while (1)
    {

        P(sem->semid1);
        if (si->sockfd == 0 && si->port == 0 && flag == 0)
        {
            // MSOCKET
            printf("Socket call\n");
            fd = socket(AF_INET, SOCK_DGRAM, 0);
            // num_sent=0;
            si->sockfd = fd;
            if (fd == -1)
            {
                si->err_no = errno;
            }
            V(sem->semid2);
        }
        else if (si->port != 0 && flag == 0)
        {
            // MBIND
            printf("Bind call\n");
            struct sockaddr_in serv_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(si->port);
            inet_aton(si->ip, &serv_addr.sin_addr);
            int sfd = si->sockfd;

            if (bind(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
            {
                printf("Unsuccessful Bind\n");
                si->err_no = errno;
                perror("");
                close(si->sockfd);
                si->sockfd = -1;
            }
            else
            {
                printf("Successful Bind\n");
            }
            V(sem->semid2);
        }
        // else if (flag == 0)
        // {
        //     pthread_mutex_lock(&mutex);
        //     printf("Entering Close call\n");
        //     close(si->sockfd);
        //     SHM[si->msockfd].free = 1;
        //     V(sem->semid2);
        //     pthread_mutex_unlock(&mutex);
        //     // printf("Signaling close\n");
        // }
    }
    shmdt(SHM);
    shmdt(sem);
    shmdt(si);
    shmctl(SHM_id, IPC_RMID, NULL);
    shmctl(SHM_id1, IPC_RMID, NULL);
    shmctl(SHM_id2, IPC_RMID, NULL);

    return 0;
}
