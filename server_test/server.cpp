#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include<iostream>
#include <pthread.h>

#define MAX 80
#define PORT 9200
#define SA struct sockaddr
using namespace std;

void* send_back(void* x)
{
    int client_socket = *((int*) x);
    cout << "client socket: " << client_socket << endl;

    char buf[4096];
    // while (1)
    // {
    //     memset(buf , 4096 , 0);
    //     int bytesReceived = recv(client_socket, buf, 4096, 0);
    //     if (bytesReceived > 0)
    //     {
    //         int sent = send(client_socket, buf, bytesReceived + 1, 0);
    //         cout << "sent_back:" << sent << "bytes to :"   << client_socket << endl;
    //     }
    // }
    // close(client_socket);

    int fd;
    char filename[] = {"a.txt"};
    char buffer[4096] ;
    //open file
    fd = open(filename, O_RDWR | O_CREAT, 0777);

    // nw = pwrite(fd, &buf_wr, strlen(buf_wr), 14);

    long nr =  read(fd, buffer, sizeof(buffer) );
    cout << "kitnasd::::" << nr << endl;
    // cout << " offset:::" << offset << endl;

    //read process error control
    if (nr == -1) {
        perror("[error in read]\n");
    } else {
        printf("[reading(1) data] from %s\n", filename);
    }

    ssize_t nw2 = write(client_socket, buffer , (int)nr);
    // cout << " offset:::" << offset << endl;

//write error checking
    if (nw2 == -1) {
        perror("[error in write 2]\n");
    }
    close(client_socket);
    close(fd);
    return 0;
}

int main()
{
    int ser_lis_socket , client_socket;
    ser_lis_socket  = socket(AF_INET, SOCK_STREAM, 0);
    if (ser_lis_socket  == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    struct sockaddr_in server, client;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    memset(server.sin_zero , 8, 0);

    unsigned len = sizeof(sockaddr_in);

    if ((bind(ser_lis_socket, (SA*)&server, sizeof(server))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    if ((listen(ser_lis_socket, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");
    while (1)
    {


        client_socket = accept(ser_lis_socket, (SA*)&client, &len);
        if (client_socket < 0) {
            printf("server accept failed...\n");
            exit(0);
        }
        else
            printf("server accept the client...\n");
        pthread_t t1;
        cout << client_socket << endl;
        pthread_create(&t1 , 0 , send_back , (void*)&client_socket);
    }


    return 0;
}