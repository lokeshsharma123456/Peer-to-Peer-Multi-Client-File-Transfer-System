#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/types.h>
#include<iostream>
#include <pthread.h>
#include <unistd.h> // read(), write(), close()
#define MAX 80
#define PORT 9200
#include<iostream>
using namespace std;


int main()
{
    int client_lis_socket  ;
    client_lis_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_lis_socket  == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");



    struct sockaddr_in  client;
    client.sin_family = AF_INET;
    // client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_addr.s_addr = inet_addr("127.0.0.1");
    client.sin_port = htons(PORT);
    memset(client.sin_zero , 8, 0);


    if (connect(client_lis_socket, (sockaddr*)&client, sizeof(client))
            != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");
    char buf[4096];
    string msg;




    // do
    // {
    //     getline(cin, msg);
    //     if (msg.length() > 0)
    //     {
    //         int sent = send(client_lis_socket, msg.c_str(), msg.size() + 1, 0);
    //         if (sent != -1)
    //         {
    //             memset(buf, 4096, 0);
    //             int bytesreceived = pread(client_lis_socket, buf, 4096, 0);
    //             if (bytesreceived > 0)
    //             {
    //                 cout << "server:" << string(buf, 0, bytesreceived) << endl;

    //             }

    //         }
    //     }
    // } while (msg.length() > 0);

    char filename[] = {"to.txt"};
    char buffer[4096] ;
    //open file
    int fd = open(filename, O_RDWR | O_CREAT, 0777);
    if (fd == -1) {
        perror("[error in read]\n");
    } else {
        printf("[reading(1) data] from %s\n", filename);
    }

    // nw = pwrite(fd, &buf_wr, strlen(buf_wr), 14);

    ssize_t nr =  read(client_lis_socket, buffer, sizeof(buffer) );
    if (nr == -1) {
        perror("[error in read]\n");
    }
    cout << "size:" << nr << endl;

    ssize_t nw2 =   write(fd, buffer, nr  );
    if (nw2 == -1)
        perror("[error in rwrite]\n");
    else
        cout << "size:" << nw2 << endl;
    close(fd);

    cout << "hello\n";


    return 0;
}