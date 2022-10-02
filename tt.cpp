
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <pthread.h>
#include <netinet/in.h>
#include<iostream>
using namespace std;
#define MAX 80
#define PORT 8080
#define SA struct sockaddr

struct Server
{
    int domain;
    int service;
    int protocol;
    u_long interface;
    int port;
    int backlog;
    struct sockaddr_in address;
    int server_lis_socket;

};

struct Server server_constructor(int domain , int service , int protocol, u_long interface, int port, int backlog)
{
    struct Server server;
    server.domain = domain;
    server.service  = service;
    server.protocol = protocol;
    server.interface = interface;
    server.port = port;
    server.backlog = backlog;

    server.address.sin_addr.s_addr = htonl(interface);
    server.address.sin_family = domain;
    server.address.sin_port = htons(port);

    server.server_lis_socket = socket(domain, service, protocol);

    if (server.server_lis_socket  == 0)
    {
        perror("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    if ((bind(server.server_lis_socket, (struct sockaddr*)&server.address, sizeof(server.address))) < 0) {
        perror("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    if ((listen(server.server_lis_socket, server.backlog)) < 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");

    return server;
}





void* server_function(void *arg)
{
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, 8080, 0);
    struct sockaddr *address = (struct sockaddr *)&server.address;
    socklen_t address_length = (socklen_t)sizeof(server.address);

    while (1)
    {
        int client_socket = accept(server.server_lis_socket , address, &address_length);
        if (client_socket < 0) {
            printf("server accept failed...\n");
            exit(0);
        }
        else
            printf("server accept the client...\n");

        char request[255];
        memset(request , 0 , 255);
        read(client_socket , request , 255);
        cout << request << endl;
        close(client_socket);
    }
    return NULL;
}

//------------------client---------------------------------------------------------------------------------


struct Client
{
    int socket;
    int domain;
    int service;
    int protocol;
    u_long interface;
    int port;
    char*(*request) (struct Client *client, char *server_ip, char *request);

};

char * request(struct Client *client, char*server_ip , char *request)
{
    struct sockaddr_in server_address;
    server_address.sin_family = client->domain;
    server_address.sin_port = htons(client->port);
    server_address.sin_addr.s_addr = (int)client->interface;

    inet_pton(client->domain , server_ip, &server_address.sin_addr);

    if (connect(client->socket, (sockaddr*)&server_address, sizeof(server_address))
            != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n") ;

    send(client->socket, request, strlen(request), 0);

    char *response = (char*)malloc(30000);
    read (client ->socket , response, 30000);
    return request;

}


struct Client client_constructor(int domain, int service , int protocol, int port , int interface)
{
    struct Client client;
    client.domain = domain;
    client.port = port ;
    client.interface = interface;

    client.socket = socket(domain, service, protocol);
    client.request = request;
    return client;
}






void client_function(char * request)
{
    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, 1248 , INADDR_ANY);
    client.request(&client, "127.0.0.1", request);
}


int main()
{
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_function, NULL);

    while (1)
    {
        char request[255];
        memset(request , 0 , 255);
        fgets(request ,  255, stdin);
        client_function(request);
    }


    return 0;
}






































