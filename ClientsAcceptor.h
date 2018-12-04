#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <poll.h>
#include "ConnectionInfo.h"
#include "ThreadPool.h"

struct ThreadRegisterInfo {
    ThreadRegisterInfo(pollfd *d, std::map<int, ConnectionInfo> *mp) : descyptor(d), mapPointer(mp) {}

    pollfd *descyptor;
    std::map<int, ConnectionInfo> *mapPointer;
};

class ClientsAcceptor {
public:
    ClientsAcceptor();

    ~ClientsAcceptor();
    explicit ClientsAcceptor(int port);
    bool listenAndRegister();
private:
    int i;
    int port;
    int serverSocket;
    int clientSocket;
    std::map<int, ConnectionInfo> connections;
    std::vector<pollfd> clients;
    struct sockaddr_in serverAddr,clientAddr;
    int CLIENT_SOCKET_SIZE;
    const static int MAXIMIUM_CLIENTS = 1024;
    const static int DEFAULT_PORT = 8080;
    ThreadPool pool;
};


