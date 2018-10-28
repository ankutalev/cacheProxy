#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


class ClientsAcceptor {
public:
    ClientsAcceptor();

    ~ClientsAcceptor();
    explicit ClientsAcceptor(int port);
    bool listenAndRegister();
private:
    int port;
    int serverSocket;
    int clientSocket;
    struct sockaddr_in serverAddr,clientAddr;
    int CLIENT_SOCKET_SIZE;
    const static int BUFFER_LENGTH = 100;
    const static int MAXIMIUM_CLIENTS = 1024;
    const static int DEFAULT_PORT = 52849;
    char buffer [BUFFER_LENGTH];
};


