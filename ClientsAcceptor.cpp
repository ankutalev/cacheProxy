#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <stdexcept>
#include <netdb.h>
#include <cstring>
#include <poll.h>
#include <fcntl.h>
#include "picohttpparser/picohttpparser.h"
#include "ClientsAcceptor.h"


bool httpParse(const std::string &req, ConnectionInfo* info) {
    const char* path;
    const char* method;
    int pret, minor_version;
    struct phr_header headers[100];
    size_t prevbuflen = 0, method_len, path_len, num_headers;
    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_request(req.c_str(), req.size(), &method, &method_len, &path, &path_len,
                             &minor_version, headers, &num_headers, prevbuflen);
    if (pret == -1)
        return false;
    std::cout << "\n\n\nSTART OF INFORMATION\n\n\n";
    printf("request is %d bytes long\n", pret);
    info->method = method;
    info->method.erase(info->method.begin() + method_len, info->method.end());
    info->path = path;
    info->path.erase(info->path.begin() + path_len, info->path.end());
    for (int i = 0; i != num_headers; ++i) {
        std::string headerName = headers[i].name;
        headerName.erase(headerName.begin() + headers[i].name_len, headerName.end());
        info->otherHeaders[headerName] = headers[i].value;
        info->otherHeaders[headerName].erase(info->otherHeaders[headerName].begin() + headers[i].value_len,
                                             info->otherHeaders[headerName].end());
        if (headerName == "Host")
            info->host = info->otherHeaders[headerName];
    }
    return true;
}


static void* writeToBrowser(void* params) {
    pollfd* info = (pollfd*) params;
    std::cerr << "ALLO!!!";
    write(info->fd, "sosi jopu", 9);
    close(info->fd);
    return NULL;
}


static void* targetConnect(void* arg) {
    TargetConnectInfo* requiredInfo = (TargetConnectInfo*) arg;
    const static int BUFFER_LENGTH = 1500;
    char buffer[BUFFER_LENGTH];
    std::fill(buffer, buffer + BUFFER_LENGTH, 0);
    std::cout << "CONNECT TO TARGET FROM: " << requiredInfo->client->fd << "\n";
    std::string request;
    while (ssize_t read = recv(requiredInfo->client->fd, buffer, BUFFER_LENGTH, 0)) {
        request += buffer;
        if (read != BUFFER_LENGTH)
            break;
    }
    std::cout << "i read " << request << std::endl;
    ConnectionInfo targetInfo;

    if (!httpParse(request, &targetInfo)) {
        std::cout << "Invalid http request received!\n";
        requiredInfo->brokenDescryptors->insert(requiredInfo->client);
        return NULL;
    }

    for (int i = 0; i < request.size(); ++i) {
        (*requiredInfo->dataPieces)[requiredInfo->target].push_back(request[i]);
    }

    addrinfo hints = {0};
    hints.ai_flags = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addr = NULL;
    getaddrinfo(targetInfo.host.c_str(), NULL, &hints, &addr);
    sockaddr_in targetAddr;
    targetAddr = *(sockaddr_in*) (addr->ai_addr);
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(80);

    int targetSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (targetSocket == -1) {
        std::cout << "Can't open target socket! Terminating" << std::endl;
        requiredInfo->brokenDescryptors->insert(requiredInfo->client);
        requiredInfo->brokenDescryptors->insert(requiredInfo->target);
        return NULL;
    }
    if (connect(targetSocket, (sockaddr*) &targetAddr, sizeof(targetAddr))) {
        std::cout << "Can't connect to target! Terminating!" << std::endl;
        requiredInfo->brokenDescryptors->insert(requiredInfo->client);
        requiredInfo->brokenDescryptors->insert(requiredInfo->target);
        return NULL;
    }
    if (fcntl(targetSocket, F_SETFL, fcntl(targetSocket, F_GETFL, 0) | O_NONBLOCK) == -1) {
        std::cout << "Can't make target socket non blocking! Terminating!" << std::endl;
        requiredInfo->brokenDescryptors->insert(requiredInfo->client);
        requiredInfo->brokenDescryptors->insert(requiredInfo->target);
        return NULL;
    }

    requiredInfo->target->fd = targetSocket;
    (*requiredInfo->transferMap)[requiredInfo->client] = (*requiredInfo->transferMap)[requiredInfo->target];
    (*requiredInfo->transferMap)[requiredInfo->target] = (*requiredInfo->transferMap)[requiredInfo->client];

    return NULL;
}


static void* acceptConnection(void* args) {
    ThreadRegisterInfo* descs = (ThreadRegisterInfo*) args;
    sockaddr_in addr;
    size_t addSize = sizeof(addr);
    int newClient = accept(*descs->server, (sockaddr*) &addr, (socklen_t*) &addSize);


    if (newClient == -1) {
        throw std::runtime_error("can't accept!");
    }
    //is this required?
    fcntl(newClient, F_SETFL, fcntl(newClient, F_GETFL, 0) | O_NONBLOCK);
    descs->client->fd = newClient;
    return NULL;
}


ClientsAcceptor::ClientsAcceptor() : pool(1) {
    port = DEFAULT_PORT;
    clientSocket = -1;

    CLIENT_SOCKET_SIZE = sizeof(clientAddr);
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    pollDescryptors = new std::vector<pollfd>;
    transferMap = new std::map<pollfd*, pollfd*>;
    dataPieces = new std::map<pollfd*, std::vector<char> >;


    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (serverSocket == -1)
        throw std::runtime_error("Can't open server socket!");

    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*) &serverAddr, sizeof(serverAddr))) {
        throw std::runtime_error("Can't bind server socket!");
    }


    if (listen(serverSocket, MAXIMIUM_CLIENTS))
        throw std::runtime_error("Can't listen this socket!");

    if (fcntl(serverSocket, F_SETFL, fcntl(serverSocket, F_GETFL, 0) | O_NONBLOCK) == -1) {
        throw std::runtime_error("Can't make server socket nonblock!");
    }

    pollfd me;
    me.fd = serverSocket;
    me.events = POLLIN;
    pollDescryptors->reserve(MAXIMIUM_CLIENTS);
    pollDescryptors->push_back(me);

}

ClientsAcceptor::ClientsAcceptor(int port) : port(port), pool(1) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    CLIENT_SOCKET_SIZE = sizeof(clientAddr);
    if (serverSocket == -1)
        throw std::runtime_error("Can't open server socket!");

    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*) &serverAddr, sizeof(serverAddr)))
        throw std::runtime_error("Can't bind server socket!");


    if (listen(serverSocket, MAXIMIUM_CLIENTS))
        throw std::runtime_error("Can't listen this socket!");


}

void removeFromPoll() {}

void reBase() {}

bool ClientsAcceptor::listenAndRegister() {
//    pool.startAll();
    while (1)
        pollManage();
}

void ClientsAcceptor::pollManage() {
    pollfd c;
    c.fd = -1;
    c.events = POLLIN;


    poll(pollDescryptors->data(), pollDescryptors->size(), POLL_DELAY);

    unsigned long oldPollSize = pollDescryptors->size();
    unsigned long descryptorsWithNoTarget = (pollDescryptors->size() - 1) - transferMap->size();

    for (unsigned long i = 0; i < descryptorsWithNoTarget; ++i) {
        pollfd target;
        target.fd = -1;
        target.events = POLLOUT;
        pollDescryptors->push_back(target);
    }

    std::vector<pollfd>::iterator noTargetsIterator = pollDescryptors->begin() + oldPollSize;

    for (std::vector<pollfd>::iterator it = pollDescryptors->begin(); it != pollDescryptors->end(); ++it) {
        if (it->fd != serverSocket and !transferMap->count(&*it)) {
            TargetConnectInfo tgc(&serverSocket, &*it, &*noTargetsIterator, dataPieces, transferMap,
                                  &brokenDescryptors);
            targetConnect(&tgc);
            ++noTargetsIterator;
        } else if (it->revents & POLLIN) {
            if (it->fd == serverSocket) {
                ThreadRegisterInfo info(&serverSocket, &c);
//                pool.addJob(acceptConnection,&info);
                acceptConnection(&info);
            } else {
//                readData(&*it);
            }
        }
    }

    removeFromPoll();
    if (pollDescryptors->size() > 10)
        reBase();


    if (c.fd != -1) {
        pollDescryptors->push_back(c);
        std::cout << "PUSHBACKUL!\n";
    }

}


ClientsAcceptor::~ClientsAcceptor() {
    close(serverSocket);
}



