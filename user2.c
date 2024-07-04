
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include "msocket.h"

#define MAXSIZE 1024


int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        printf("Rerun the application ./user2 with the following arguments\n");
        printf("1. Self IP\n");
        printf("2. Self port\n");
        printf("3. Destination IP\n");
        printf("4. Destination port\n");
        printf("5. File path\n");
        printf("For example---> ./user2 127.0.0.1 60000 127.0.0.1 50000 new.txt\n");
        return 0;
    }
    char filepath[1000];
    if (argc == 5)
    {
        printf("Enter path of new file: ");
        scanf("%s", filepath);
    }
    else
    {
        strcpy(filepath, argv[5]);
    }

    int fd = open(filepath, O_CREAT|O_RDWR,0666);
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

    printf("Started writing in file %s\n",filepath);
    // Receive the file over the MTP socket
    char buffer[MAXSIZE];
    while(1){
        int n=m_recvfrom(sockfd,buffer,MAXSIZE,0,argv[3],dest_port);
        if(n==-1){
            sleep(1);
            continue;
        }
        else if(n==1 && buffer[0]=='\0'){
            printf("Written into file successfully\n");
            break;
        }
        else{
            int len=write(fd,buffer,n);
            if(len==-1){
                perror("Error in writing: ");
            }
            else{
                printf("Written length %d\n",len);
            }
        }
    }

    // Close the file and the MTP socket
    close(fd);
    // Giving enough time to complete the conversation before the user2 process terminates
    sleep(10*T);
    return 0;
}


