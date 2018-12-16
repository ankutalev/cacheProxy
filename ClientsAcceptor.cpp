#pragma ide diagnostic ignored "modernize-use-auto"
#pragma ide diagnostic ignored "modernize-use-nullptr"

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


enum ResponseParseStatus {
    OK, NoCache, Error
};

static void registerForWrite(std::vector<pollfd>::iterator* it) {
    (*it)->events = POLLOUT;
}

static void registerForWrite(pollfd* fd) {
    fd->events = POLLOUT;
}


static void removeFromPoll(std::vector<pollfd>::iterator* it) {
    close((*it)->fd);
    (*it)->fd = -(*it)->fd;
}


bool httpParseRequest(std::string &req, ConnectionInfo* info) {
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
    std::cout << "REQUESt IS" << req << std::endl;
    printf("request is %d bytes long\n", pret);
    info->method = method;
    info->method.erase(info->method.begin() + method_len, info->method.end());
    info->path = path;
    info->path.erase(info->path.begin() + path_len, info->path.end());
    for (int i = 0; i != num_headers; ++i) {
        std::string headerName = headers[i].name;
        headerName.erase(headerName.begin() + headers[i].name_len, headerName.end());
        if (headerName != "Connection") {
            info->otherHeaders[headerName] = headers[i].value;
            info->otherHeaders[headerName].erase(info->otherHeaders[headerName].begin() + headers[i].value_len,
                                                 info->otherHeaders[headerName].end());
        }
        if (headerName == "Connection") {
            std::cout << "got you";
        }
        if (headerName == "Host")
            info->host = info->otherHeaders[headerName];
    }
    std::cout << "finish" << std::endl;
    std::string changedRequest = info->method + " " + info->path + " " + "HTTP/1.0\r\n";
    for (std::map<std::string, std::string>::iterator it = info->otherHeaders.begin();
         it != info->otherHeaders.end(); ++it) {
        changedRequest += it->first;
        changedRequest += ": ";
        changedRequest += it->second;
        changedRequest += "\r\n";
    }
    changedRequest += "\r\n";
    req = changedRequest;
    return true;
}

//"GET http://lib.ru/ HTTP/1.1\r\nHost: lib.ru\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:64.0) Gecko/20100101 Firefox/64.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nConnection: keep-alive\r\nUpgrade-Insecure-Requests: 1\r\nIf-Modified-Since: Tue, 13 Nov 2018 17:58:28 GMT\r\nCache-Control: max-age=0\r\n\r\n"

ResponseParseStatus httpParseResponse(const char* response, size_t responseLen) {
    const char* message;
    int pret, minor_version, status;
    struct phr_header headers[100];
    size_t prevbuflen = 0, message_len, num_headers;
    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_response(response, responseLen, &minor_version, &status, &message, &message_len,
                              headers, &num_headers, prevbuflen);

    std::cout << "RESPONSE IS" << response << std::endl;
    if (pret == -1)
        return Error;
//    std::cout << "RESPONSE IS " << response << std::endl;
    std::cout << "VERSION " << minor_version << " STATUS " << status << std::endl;
    return (status == 200) ? OK : NoCache;
}


static void* writeToClient(void* arg) {
    TargetConnectInfo* requiredInfo = (TargetConnectInfo*) arg;
    pollfd* client = &**requiredInfo->clientIterator;
    std::string gettingPath = (*requiredInfo->descsToPath)[client].path;

    //если есть в кеше
    if (requiredInfo->cacheLoaded->count(gettingPath)) {
        //проверяем, загружен ли кеш, если нет - выходим пока
        bool isCacheReady = (*requiredInfo->cacheLoaded)[gettingPath];
        if (!isCacheReady)
            return NULL;
        else {
            std::cout << "CACHE SIZE IS " << (*requiredInfo->cache)[gettingPath].size() << std::endl;
            ssize_t s = send(client->fd, &(*requiredInfo->cache)[gettingPath].front(),
                             (*requiredInfo->cache)[gettingPath].size(), 0);
            perror("SEND");
            std::cout << "SENDED FROM CACHE" << s << std::endl;
            if (s == 0) {
                std::cout << "AAAA???" << std::endl;
            }
        }
    }
        //иначе проверяем дату, которая у нас есть
    else if (requiredInfo->dataPieces->count(client)) {
        std::cout << "DATA IS " << &(*requiredInfo->dataPieces)[client].front() << std::endl;
        std::cout << "DATA SIZE IS " << (*requiredInfo->dataPieces)[client].size() << std::endl;
        ssize_t s = send(client->fd, &(*requiredInfo->dataPieces)[client].front(),
                         (*requiredInfo->dataPieces)[client].size(), 0);
        std::cout << "SENDED " << s << std::endl;

    }
        //иначе данные о кеше были удалены во время закачки (оборвалась клиентская сессия, качающая кеш, нужно отдельно мансить
    else {
        std::cerr << "PIzDA TUT";
    }
    //
    removeFromPoll(requiredInfo->clientIterator);
    std::cerr << "SENDED!!!";
    perror("what");
    return NULL;
}

static void* targetConnect(void* arg) {
    TargetConnectInfo* requiredInfo = (TargetConnectInfo*) arg;
    const static int BUFFER_LENGTH = 5000;
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
        std::cerr << "Invalid http request received!" << std::endl;
        removeFromPoll(requiredInfo->clientIterator);
        return NULL;
    }

    if (targetInfo.method != "GET" and targetInfo.method != "HEAD") {
        std::string notSupporting = "HTTP/1.1 405\r\n\r\nAllow: GET\r\n";
        send((*requiredInfo->clientIterator)->fd, notSupporting.c_str(), notSupporting.size(), 0);
        removeFromPoll(requiredInfo->clientIterator);
        std::cerr << "Method not allowed!" << std::endl;
        return NULL;
    }

    typeConnectionAndPath metaInfo;
    metaInfo.path = targetInfo.path;
    metaInfo.isClient = true;
    (*requiredInfo->descsToPath)[oldClientAddress] = metaInfo;


    if (requiredInfo->cacheLoaded->count(targetInfo.path)) {
        registerForWrite(requiredInfo->clientIterator);
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
        removeFromPoll(requiredInfo->clientIterator);
        return NULL;
    }

    targetAddr = *(sockaddr_in*) (addr->ai_addr);
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(80);

    int targetSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (targetSocket == -1) {
        std::cerr << "Can't open target socket! Terminating" << std::endl;
        removeFromPoll(requiredInfo->clientIterator);
        return NULL;
    }


    if (connect(targetSocket, (sockaddr*) &targetAddr, sizeof(targetAddr)) != 0 and errno != EINPROGRESS) {
        std::cerr << "Can't async connect to target! Terminating!" << std::endl;
        removeFromPoll(requiredInfo->clientIterator);
        return NULL;
    }


    pollfd target;
    target.fd = targetSocket;
    target.events = POLLOUT;

    *requiredInfo->clientIterator = requiredInfo->pollDescryptos->insert(*requiredInfo->clientIterator + 1, target);
    pollfd* insertedAddress = &**requiredInfo->clientIterator;

    std::cout << "old " << oldClientAddress << " inserted " << insertedAddress << " insrted fd " << insertedAddress->fd
              << std::endl;



    for (int i = 0; i < request.size(); ++i) {
        (*requiredInfo->dataPieces)[insertedAddress].push_back(request[i]);
    }


    (*requiredInfo->transferMap)[oldClientAddress] = insertedAddress;
    (*requiredInfo->transferMap)[insertedAddress] = oldClientAddress;

    metaInfo.isClient = false;

    (*requiredInfo->descsToPath)[insertedAddress] = metaInfo;
    return NULL;
}

static void* readFromServer(void* arg) {
    TargetConnectInfo* requiredInfo = (TargetConnectInfo*) arg;
    pollfd* addr = &**requiredInfo->clientIterator;

    std::cout << "read from server" << std::endl;
    if (!requiredInfo->transferMap->count(addr)) {
        std::cerr << "No client to write from server!";
        removeFromPoll(requiredInfo->clientIterator);
        return NULL;
    }

    pollfd* to = (*requiredInfo->transferMap)[addr];

    const static int BUFFER_SIZE = 5000;
    char buffer[BUFFER_SIZE];
    std::fill(buffer, buffer + BUFFER_SIZE, 0);
    std::string response;

    //todo сделать нормальное покусочное считывание
    while (1) {
        ssize_t readed = recv(addr->fd, buffer, BUFFER_SIZE, 0);
        std::cout << readed << std::endl;
        for (int i = 0; i < readed; ++i) {
            (*requiredInfo->dataPieces)[to].push_back(buffer[i]);
        }

        if (readed == -1 and errno != EWOULDBLOCK) {
            //huynya proizoshla
            perror("HUYNYA");
        }
        (*requiredInfo->cacheLoaded)[(*requiredInfo->descsToPath)[addr].path] = false;
        //считали все с сервера
        if (!readed) {
            (*requiredInfo->cacheLoaded)[(*requiredInfo->descsToPath)[addr].path] = true;
            ResponseParseStatus status = httpParseResponse(&(*requiredInfo->dataPieces)[to].front(),
                                                           (*requiredInfo->dataPieces)[to].size());
            if ((*requiredInfo->dataPieces)[to].empty()) {
                std::cout << "why empty?";
            }
            std::string serverError = "HTTP/1.1 523\r\n\r\n";
            switch (status) {
                case OK:
                    //ок - записать в кеш, удлаить из сообщений todo удалить из трансфер мапы? а в сендклиент смотреть сначала на кеш а потом на присутсвие в мапе
                    (*requiredInfo->cache)[(*requiredInfo->descsToPath)[addr].path].swap(
                            (*requiredInfo->dataPieces)[to]);
                    requiredInfo->dataPieces->erase(to);
                    requiredInfo->transferMap->erase(to);
                    requiredInfo->transferMap->erase(addr);
                    break;
                case Error:
                    //ошибка при парсинге - удаляем всю информацию о том,что мы были на этой странице и закрываем соединение
                    (*requiredInfo->cacheLoaded).erase((*requiredInfo->descsToPath)[addr].path);
                    removeFromPoll(requiredInfo->clientIterator);
                    for (int i = 0; i < serverError.size(); ++i) {
                        (*requiredInfo->dataPieces)[to].push_back(serverError[i]);
                    }
                    break;
                case NoCache:
                    (*requiredInfo).cacheLoaded->erase((*requiredInfo->descsToPath)[addr].path);
                    break;
            }
            removeFromPoll(requiredInfo->clientIterator);
            registerForWrite(to);
            return NULL;
        }
        if (errno == EWOULDBLOCK) {
//            removeFromPoll(requiredInfo->clientIterator);
            std::cout << "EWOUDLBLOCK" << std::endl;
//            registerForWrite(to);
            return NULL;
        }

    }

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

    std::cout << "I SENDIND THIS" << &(*requiredInfo->dataPieces)[requiredInfo->target][0] << std::endl;


    do {
        sended = send(requiredInfo->target->fd, &(*requiredInfo->dataPieces)[requiredInfo->target][0],
                      (*requiredInfo->dataPieces)[requiredInfo->target].size(), 0);
        (*requiredInfo->dataPieces)[requiredInfo->target].erase(
                (*requiredInfo->dataPieces)[requiredInfo->target].begin(),
                (*requiredInfo->dataPieces)[requiredInfo->target].begin() + sended);
    } while (sended > 0);

    requiredInfo->dataPieces->erase(requiredInfo->target);
    requiredInfo->target->events = POLLIN;
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

    poll(&(*pollDescryptors)[0], pollDescryptors->size(), 500);


    for (std::vector<pollfd>::iterator it = pollDescryptors->begin(); it != pollDescryptors->end(); ++it) {

        std::cout << "NOW WORK WITH DESCR" << it->fd << " AND EVENTS " << it->events << "AND REVENTS " << it->revents
                  << std::endl;

        if (it->fd > 0 and it->revents & POLLIN) {
            //если слушающий сокет  - принимаем конекшон
            if (it->fd == serverSocket) {
                ThreadRegisterInfo info(&serverSocket, &c);
                acceptConnection(&info);
                //если сокет - клиент
            } else if (!descsToPath.count(&*it)) {
                TargetConnectInfo tgc(&serverSocket, &it, pollDescryptors, dataPieces, transferMap, &descsToPath,
                                      &cacheLoaded, &cache);
                targetConnect(&tgc);
            }
                //иначе  считываем инфу с сервера
            else {
                TargetConnectInfo tgc(&serverSocket, &it, pollDescryptors, dataPieces, transferMap, &descsToPath,
                                      &cacheLoaded, &cache);
                readFromServer(&tgc);
            }

        } else if (it->revents & POLLOUT) {
            SendDataInfo sdi(&*it, dataPieces, pollDescryptors);
            if (!descsToPath[&*it].isClient) {
                sendData(&sdi);
                std::cout << "vislal na server!" << std::endl;
            } else {
                std::cout << "rabotaem s clientami!!";
                TargetConnectInfo tgc(&serverSocket, &it, pollDescryptors, dataPieces, transferMap, &descsToPath,
                                      &cacheLoaded, &cache);
                writeToClient(&tgc);
            }
            std::cout << "WOW";
        }
    }

    for (int j = 0; j < pollDescryptors->size(); ++j) {
        std::cout << (*pollDescryptors)[j].fd << " " << (*pollDescryptors)[j].events << " "
                  << (*pollDescryptors)[j].revents << std::endl;
    }


//    if (pollDescryptors->size() > 10)
//

    if (c.fd != -1) {
        pollDescryptors->push_back(c);
    }

}


ClientsAcceptor::~ClientsAcceptor() {
    close(serverSocket);
}



