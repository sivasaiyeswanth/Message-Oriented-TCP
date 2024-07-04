
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include<fcntl.h>
#include "msocket.h"

#define MAXSIZE 1024
//int num_messages=0;
int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        printf("Rerun the application ./user1 with the following arguments\n");
        printf("1. Self IP\n");
        printf("2. Self port\n");
        printf("3. Destination IP\n");
        printf("4. Destination port\n");
        printf("5. File path\n");
        printf("For example---> ./user1 127.0.0.1 50000 127.0.0.1 60000 text1.txt\n");
        return 0;
    }
    char filepath[1000];
    if (argc == 5)
    {
        printf("Enter file path: ");
        scanf("%s", filepath);
    }
    else
    {
        strcpy(filepath, argv[5]);
    }

    int fd = open(filepath, O_RDONLY,0666);
    if (fd == -1)
    {
        perror("Error in file opening: ");
        return 1;
    }

    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sockfd == -1)
    {
        perror("Error in m_socket: ");
        return 1;
    }
    printf("Socket Created\n");

    int my_port = atoi(argv[2]);
    int dest_port = atoi(argv[4]);

    if (m_bind(sockfd, argv[1], my_port, argv[3], dest_port) == -1)
    {
        perror("Error in m_bind: ");
        return 1;
    }
    printf("Bind Successful\n");
    printf("Started reading from file %s and sending\n",filepath);
    char buffer[MAXSIZE];
    while (1)
    {
        int len = read(fd, buffer, MAXSIZE);
        int flag = 0;
        if (len == -1)
        {
            perror("Error in reading: ");
            break;
        }
        else if (len == 0)
        {
            buffer[0] = '\0';
            len = 1;
            flag = 1;
        }
        while (1)
        {
            int n = m_sendto(sockfd, buffer, len, 0, argv[3], dest_port);
            if (n == -1)
            {   
                perror("Error in sending: ");
                sleep(3);
                continue;
            }
            else{
               // num_messages++;
                printf("Sent length %d\n",n);
                break;
            }
        }
        if (flag)
        {
            printf("File sent successfully\n");
            break;
        }
    }

    close(fd);
    // Giving enough time to complete the conversation before the user1 process terminates
    sleep(10*T);
    return 0;
}
