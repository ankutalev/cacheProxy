#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <stdexcept>
#include <netdb.h>
#include <cstring>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include "picohttpparser/picohttpparser.h"
#include "ClientsAcceptor.h"


bool httpParseRequest(const std::string &req, ConnectionInfo* info) {
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

static void* readData(void* arg) {
//    pollfd* server = (pollfd*) arg;
//    if (!transferPipes->count(idDesc)) {
//        brokenDescryptors.insert(idDesc);
//        return;
//    }
//    pollfd *to = (*transferPipes)[idDesc];
//
//    while (1) {
//        auto readed = recv(idDesc->fd, buff, BUFFER_SIZE - 1, 0);
//        buff[readed] = 0;
//        if (!readed) {
//            registerDescryptorForPollWrite(to);
//            registerDescryptorForPollWrite(to);
//            brokenDescryptors.insert(idDesc);
//            return;
//        }
//        if (errno == EWOULDBLOCK) {
//            errno = EXIT_SUCCESS;
//            registerDescryptorForPollWrite(to);
//            return;
//        }
//        for (int i = 0; i < readed; ++i) {
//            (*dataPieces)[to].emplace_back(buff[i]);
//        }
//    }

    return NULL;
}

static void* targetConnect(void* arg) {
    TargetConnectInfo* requiredInfo = (TargetConnectInfo*) arg;
    const static int BUFFER_LENGTH = 1500;
    char buffer[BUFFER_LENGTH];
    std::fill(buffer, buffer + BUFFER_LENGTH, 0);
    std::cout << "CONNECT TO TARGET FROM: " << (*requiredInfo->clientIterator)->fd << "\n";
    std::string request;
    pollfd* oldClientAddress = &**requiredInfo->clientIterator;

    while (ssize_t read = recv((*requiredInfo->clientIterator)->fd, buffer, BUFFER_LENGTH, 0)) {
        request += buffer;
        if (read != BUFFER_LENGTH)
            break;
    }
    std::cout << "i read " << request << std::endl;
    ConnectionInfo targetInfo;

    if (!httpParseRequest(request, &targetInfo)) {
        std::cout << "Invalid http request received!\n";
        requiredInfo->brokenDescryptors->insert(oldClientAddress);
        return NULL;
    }

    if (targetInfo.method != "GET" or targetInfo.method != "HEAD") {
        std::string notSupporting = "HTTP/1.1 405\r\n\r\nAllow: GET\r\n";
        send((*requiredInfo->clientIterator)->fd, notSupporting.c_str(), notSupporting.size(), 0);
        requiredInfo->brokenDescryptors->insert(oldClientAddress);
        std::cerr << "NAPISAL!";
        return NULL;
    }

    addrinfo hints = {0};
    hints.ai_flags = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addr = NULL;
    getaddrinfo(targetInfo.host.c_str(), NULL, &hints, &addr);
    sockaddr_in targetAddr;

    if (!addr) {
        std::cerr << "Can't resolve host!" << std::endl;
        std::string notSupporting = "HTTP/1.1 523\r\n\r\n";
        send((*requiredInfo->clientIterator)->fd, notSupporting.c_str(), notSupporting.size(), 0);
        requiredInfo->brokenDescryptors->insert(oldClientAddress);
        return NULL;
    }

    targetAddr = *(sockaddr_in*) (addr->ai_addr);
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(80);

    int targetSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (targetSocket == -1) {
        std::cout << "Can't open target socket! Terminating" << std::endl;
        requiredInfo->brokenDescryptors->insert(oldClientAddress);
        return NULL;
    }


    if (connect(targetSocket, (sockaddr*) &targetAddr, sizeof(targetAddr)) != 0 and errno != EINPROGRESS) {
        std::cout << "Can't async connect to target! Terminating!" << std::endl;
        requiredInfo->brokenDescryptors->insert(oldClientAddress);
        return NULL;
    }


    pollfd target;
    target.fd = targetSocket;
    target.events = POLLOUT;

    *requiredInfo->clientIterator = requiredInfo->pollDescryptos->insert(*requiredInfo->clientIterator + 1, target);

    pollfd* insertedAddress = &**requiredInfo->clientIterator;


    for (int i = 0; i < request.size(); ++i) {
        (*requiredInfo->dataPieces)[insertedAddress].push_back(request[i]);
    }


    (*requiredInfo->transferMap)[oldClientAddress] = (*requiredInfo->transferMap)[insertedAddress];
    (*requiredInfo->transferMap)[insertedAddress] = (*requiredInfo->transferMap)[oldClientAddress];
    (*requiredInfo->hostToGets)[&target] = targetInfo.path;
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

static void* sendData(void* args) {

    SendDataInfo* requiredInfo = (SendDataInfo*) args;
    ssize_t sended = 0;
    ssize_t size = (*requiredInfo->dataPieces)[requiredInfo->target].size();

    do {
        sended = send(requiredInfo->target->fd, &(*requiredInfo->dataPieces)[requiredInfo->target][0],
                      (*requiredInfo->dataPieces)[requiredInfo->target].size(), 0);
        (*requiredInfo->dataPieces)[requiredInfo->target].erase(
                (*requiredInfo->dataPieces)[requiredInfo->target].begin(),
                (*requiredInfo->dataPieces)[requiredInfo->target].begin() + sended);
    } while (sended > 0);
    requiredInfo->dataPieces->erase(requiredInfo->target);
    for (std::vector<pollfd>::iterator it = requiredInfo->pollDescryptors->begin();
         it != requiredInfo->pollDescryptors->end();) {
        if (&*it == requiredInfo->target) {
            (&*it)->events ^= POLLOUT;
            return NULL;
        } else ++it;
    }

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
    while (1)
        pollManage();
}


void ClientsAcceptor::pollManage() {
    pollfd c;
    c.fd = -1;
    c.events = POLLIN;

    poll(&(*pollDescryptors)[0], pollDescryptors->size(), POLL_DELAY);


    for (std::vector<pollfd>::iterator it = pollDescryptors->begin(); it != pollDescryptors->end(); ++it) {

        std::cout << "NOW WORK WITH DESCR" << it->fd << " AND EVENTS " << it->events << "AND REVENTS " << it->revents
                  << std::endl;

        if (it->fd > 0 and it->revents & POLLIN and it->fd != serverSocket and !transferMap->count(&*it)) {
            TargetConnectInfo tgc(&serverSocket, &it, pollDescryptors, dataPieces, transferMap,
                                  &brokenDescryptors, &hostsToGets);
            targetConnect(&tgc);
        } else if (it->revents & POLLIN) {
            if (it->fd == serverSocket) {
                ThreadRegisterInfo info(&serverSocket, &c);
                acceptConnection(&info);
            } else {
//                readData(&*it)
            }
        } else if (it->revents & POLLOUT) {
            SendDataInfo sdi(&*it, dataPieces, pollDescryptors);
            if (hostsToGets.count(&*it)) {
                sendData(&sdi);
                std::cout << "vislal na server!";
            } else {
                std::cout << "v keshe poishi!";
            }
            std::cout << "WOW";
        }
    }

    for (int j = 0; j < pollDescryptors->size(); ++j) {
        std::cout << (*pollDescryptors)[j].fd << " " << (*pollDescryptors)[j].events << " "
                  << (*pollDescryptors)[j].revents << std::endl;
    }

    removeFromPoll();

//    if (pollDescryptors->size() > 10)
//

    if (c.fd != -1) {
        pollDescryptors->push_back(c);
    }

}


ClientsAcceptor::~ClientsAcceptor() {
    close(serverSocket);
}


void ClientsAcceptor::removeFromPoll() {

    for (std::vector<pollfd>::iterator it = pollDescryptors->begin(); it != pollDescryptors->end();) {
        if (brokenDescryptors.count(&*it) and it->fd > 0) {
            close(it->fd);
            std::cout << "CLOSE this! " << it->fd << std::endl;
            it->fd = -it->fd;
        } else ++it;
    }
}


