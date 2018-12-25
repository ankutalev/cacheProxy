#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-loop-convert"
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
#include <sys/time.h>
#include <signal.h>
#include "CacheProxy.h"
#include "utils.h"


static void registerForWrite(pollfd* fd) {
    fd->events = POLLOUT;
}


static void removeFromPoll(std::vector<pollfd>::iterator* it) {
    if (close((*it)->fd)) {
        std::cout << it << std::endl;
        std::cout << (*it)->fd << std::endl;
        perror("close");
    }
    (*it)->fd = -(*it)->fd;
}


void CacheProxy::writeToClient(std::vector<pollfd>::iterator* clientIterator) {
    pollfd* client = &**clientIterator;
    std::string gettingPath = descsToPath[client].path;
    int g = client->fd;
    std::cout << "SENDED TO CLIENT" << g << std::endl;
    if (cacheLoaded.count(gettingPath)) {
        //если есть в кеше - смотрим на уровень закачки, если готов - берем, не готов - вываливаемся
        bool isCacheReady = cacheLoaded[gettingPath];
        if (!isCacheReady) {
            std::cout << "cache not ready" << std::endl;
            cacheWaits[client] = descsToPath[client].path;
            std::cout << "CACHE WAITS IS" << client << descsToPath[client].path << std::endl;
            return;
        } else {
            std::cout << "CACHE SIZE IS " << cache[gettingPath].size() << std::endl;
            //при отсутствующей записи в map c++ гарантирует что при первом обращении я получу 0
            int lastPosition = lastSendingPositionFromCache[client];
            while (lastPosition < cache[gettingPath].size()) {
                ssize_t s = send(client->fd, &cache[gettingPath].front() + lastPosition,
                                 cache[gettingPath].size() - lastPosition, 0);
                if (s == -1) {
                    if (errno != EWOULDBLOCK)
                        removeFromPoll(clientIterator);
                    return;
                }
                lastPosition += s;
                std::cout << "SENDED FROM CACHE" << s << std::endl;
                lastSendingPositionFromCache[client] = lastPosition;
            }
        }
        lastSendingPositionFromCache.erase(client);
    }
        //нет в кеше - берем сообщение из дата стора
    else if (dataPieces->count(client)) {
        std::cout << "DATA IS " << &(*dataPieces)[client].front() << std::endl;
        std::cout << "DATA SIZE IS " << (*dataPieces)[client].size() << std::endl;
        if ((*dataPieces)[client].empty()) {
            std::cout << "a";
        }
        while (not(*dataPieces)[client].empty()) {
            ssize_t s = send(client->fd, &(*dataPieces)[client].front(), (*dataPieces)[client].size(), 0);
            std::cout << "SENDED " << s << std::endl;
            if (s == -1)
                return;
            (*dataPieces)[client].erase((*dataPieces)[client].begin(), (*dataPieces)[client].begin() + s);
        }
    } else {
        //todo нет ни в дата сторе ни в кеше - дропнули клиента, который качал что-то большое, гыгы
    }
    removeFromPoll(clientIterator);
}

void CacheProxy::targetConnect(std::vector<pollfd>::iterator* clientIterator) {
    std::fill(buffer, buffer + BUFFER_LENGTH, 0);
    std::cout << "CONNECT TO TARGET FROM: " << (*clientIterator)->fd << std::endl;
    std::string request;
    pollfd* oldClientAddress = &**clientIterator;

    while (ssize_t read = recv((*clientIterator)->fd, buffer, BUFFER_LENGTH, 0)) {
        request += buffer;
        if (read != BUFFER_LENGTH)
            break;
    }

    if (request.empty()) {
        std::cout << "Dead client" << std::endl;
        removeFromPoll(clientIterator);
        return;
    }
    std::cout << "i read " << request << std::endl;
    RequestInfo targetInfo;

    if (REQ_ERROR == httpParseRequest(request, &targetInfo)) {
        std::cout << "Invalid http request received!" << std::endl;
        removeFromPoll(clientIterator);
        return;
    }

    if (targetInfo.method != "GET" and targetInfo.method != "HEAD") {
        std::string notSupporting = "HTTP/1.1 405\r\n\r\nAllow: GET\r\n";
        send((*clientIterator)->fd, notSupporting.c_str(), notSupporting.size(), 0);
        removeFromPoll(clientIterator);
        return;
    }

    typeConnectionAndPath metaInfo;
    metaInfo.path = targetInfo.path;
    metaInfo.isClient = true;
    descsToPath[oldClientAddress] = metaInfo;


    if (cacheLoaded.count(targetInfo.path)) {
        registerForWrite(oldClientAddress);
        return;
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
        std::cout << "Can't resolve host!" << std::endl;
        std::string notSupporting = "HTTP/1.1 523\r\n\r\n";
        send((*clientIterator)->fd, notSupporting.c_str(), notSupporting.size(), 0);
        removeFromPoll(clientIterator);
        return;
    }

    targetAddr = *(sockaddr_in*) (addr->ai_addr);
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(80);

    int targetSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (targetSocket == -1) {
        std::cout << "Can't open target socket! Terminating" << std::endl;
        removeFromPoll(clientIterator);
        return;
    }
    fcntl(targetSocket, F_SETFL, fcntl(targetSocket, F_GETFL, 0) | O_NONBLOCK);
    if (connect(targetSocket, (sockaddr*) &targetAddr, sizeof(targetAddr)) != 0 and errno != EINPROGRESS) {
        std::cout << "Can't async connect to target! Terminating!" << std::endl;
        removeFromPoll(clientIterator);
        return;
    }
    std::cout << "AND MY TARGET IS " << targetSocket << std::endl;


    //вставка нового дескриптора в полл

    pollfd target;
    target.fd = targetSocket;
    target.events = POLLOUT;
    target.revents = 0;
    *clientIterator = pollDescryptors->insert(pollDescryptors->end(), target);


    pollfd* insertedAddress = &**clientIterator;
    for (std::vector<pollfd>::iterator it = pollDescryptors->begin(); it != pollDescryptors->end(); ++it) {
        if (&*it == oldClientAddress) {
            *clientIterator = it;
            break;
        }
    }


    //говоришь что в этот дескриптор доллжен быть передан запрос
    for (int i = 0; i < request.size(); ++i) {
        (*dataPieces)[insertedAddress].push_back(request[i]);
    }


    (*transferMap)[oldClientAddress] = insertedAddress;
    (*transferMap)[insertedAddress] = oldClientAddress;

    metaInfo.isClient = false;

    descsToPath[insertedAddress] = metaInfo;
    (*clientIterator)->events = 0;
    return;
}

void CacheProxy::readFromServer(std::vector<pollfd>::iterator* clientIterator) {
    pollfd* addr = &**clientIterator;
    std::cout << "read from server" << std::endl;
    if (!transferMap->count(addr)) {
        std::cout << "No client to write from server!" << std::endl;
        removeFromPoll(clientIterator);
        return;
    }

    pollfd* to = (*transferMap)[addr];

    std::fill(buffer, buffer + BUFFER_LENGTH, 0);
    std::string response;


    while (1) {
        ssize_t readed = recv(addr->fd, buffer, BUFFER_LENGTH, 0);
        std::cout << readed << std::endl;
        for (int i = 0; i < readed; ++i) {
            (*dataPieces)[to].push_back(buffer[i]);
        }

        if (readed == -1 and errno != EWOULDBLOCK) {
            perror("Error during read");
            removeFromPoll(clientIterator);
            return;
        }
        cacheLoaded[descsToPath[addr].path] = false;
//        сервер разорвал соединение - докачали все
        if (!readed) {
            (cacheLoaded)[descsToPath[addr].path] = true;
            clearCacheWaits(descsToPath[addr].path);
            ResponseParseStatus status = httpParseResponse(&(*dataPieces)[to].front(), (*dataPieces)[to].size());
            std::string serverError = "HTTP/1.1 523\r\n\r\n";

            switch (status) {
                case OK:
                    //ок - докачали все, кладем в кеш
                    cache[descsToPath[addr].path].swap((*dataPieces)[to]);
                    dataPieces->erase(to);
                    transferMap->erase(to);
                    transferMap->erase(addr);
                    to->events = POLLOUT;
                    break;
                case Error:
                    //некорректный запрос - удаляем все информацию о соединениях
                    cacheLoaded.erase(descsToPath[addr].path);
                    removeFromPoll(clientIterator);
                    for (int i = 0; i < serverError.size(); ++i) {
                        (*dataPieces)[to].push_back(serverError[i]);
                    }
                    return;
                case NoCache:
                    cacheLoaded.erase(descsToPath[addr].path);
                    break;
            }
            removeFromPoll(clientIterator);
            registerForWrite(to);
            return;
        }
        if (errno == EWOULDBLOCK) {
            std::cout << "EWOUDLBLOCK" << std::endl;
            return;
        }

    }

}


void CacheProxy::acceptConnection(pollfd* client) {
    sockaddr_in addr;
    size_t addSize = sizeof(addr);
    int newClient = accept(serverSocket, (sockaddr*) &addr, (socklen_t*) &addSize);
    std::cout << "I ACCEPTED NEW CLIENT AND FD IS " << newClient << std::endl;

    if (newClient == -1) {
        throw std::runtime_error("can't accept!");
    }
    //is this required?
    fcntl(newClient, F_SETFL, fcntl(newClient, F_GETFL, 0) | O_NONBLOCK);
    client->fd = newClient;
}

void CacheProxy::sendData(std::vector<pollfd>::iterator* target) {
    if (!dataPieces->count(&**target)) {
        std::cout << "Unknown client" << std::endl;
        removeFromPoll(target);
        return;
    }
    ssize_t size = (*dataPieces)[&**target].size();
    std::cout << "with size " << size << std::endl;
    if (size == 0) {
        removeFromPoll(target);
        return;
    }
    std::cout << "I SENDIND THIS " << &(*dataPieces)[&**target][0] << std::endl;
    std::cout << "SENDING TO " << &(*target)->fd << std::endl;

    send((*target)->fd, &(*dataPieces)[&**target][0], (*dataPieces)[&**target].size(), 0);

    dataPieces->erase(&**target);
    (*target)->events = POLLIN;
    return;
}

CacheProxy::CacheProxy() {
    init(DEFAULT_PORT);
}

CacheProxy::CacheProxy(int port) : port(port) {
    init(port);
}

void CacheProxy::startWorking() {
    while (1)
        pollManage();
}


void CacheProxy::pollManage() {
    pollfd c;
    c.fd = -1;
    c.events = POLLIN;
    c.revents = 0;

    poll(&(*pollDescryptors)[0], pollDescryptors->size(), POLL_DELAY);


    for (std::vector<pollfd>::iterator it = pollDescryptors->begin(); it != pollDescryptors->end(); ++it) {

        //если валидный
        if (it->fd > 0) {
            //если ошибка (конекшен рефьюзед)
            if (it->revents & POLLERR) {
                pollfd* client = (*transferMap)[&*it];
                close(client->fd);
                client->fd = (-client->fd);
                removeFromPoll(&it);
                std::cout << "REFUSED" << std::endl;
            } else if (it->revents & POLLOUT) {
                if (!descsToPath[&*it].isClient) {
                    sendData(&it);
                } else {
                    if (!cacheWaits.count(&*it))
                        writeToClient(&it);
                }
            } else if (it->revents & POLLIN) {
                //если слушающий сокет - принимаем соединения
                if (it->fd == serverSocket) {
                    acceptConnection(&c);
                    //если это клиент - коннектимся
                } else if (!descsToPath.count(&*it) or descsToPath[&*it].isClient) {
                    targetConnect(&it);
                }
                    //если сервер - считываем инфу
                else {
                    readFromServer(&it);
                }
            }
        }
    }


    if (c.fd != -1) {
        pollDescryptors->push_back(c);
    }

    removeDeadDescryptors();
}


CacheProxy::~CacheProxy() {
    close(serverSocket);
    delete this->transferMap;
    delete this->pollDescryptors;
}

void CacheProxy::removeDeadDescryptors() {
    std::vector<pollfd>* npollDescryptors = new std::vector<pollfd>;
    npollDescryptors->reserve(MAXIMIUM_CLIENTS);

    std::map<pollfd*, pollfd*>* ntransferPipes = new std::map<pollfd*, pollfd*>;
    std::map<pollfd*, std::vector<char> >* ndataPieces = new std::map<pollfd*, std::vector<char> >;
    std::map<pollfd*, pollfd*> oldNewMap;

    for (std::vector<pollfd>::iterator it = pollDescryptors->begin(); it != pollDescryptors->end(); ++it) {
        if (it->fd > 0) {
            npollDescryptors->push_back(*it);
            oldNewMap[&*it] = &npollDescryptors->back();
        }
    }


    for (std::map<pollfd*, pollfd*>::iterator it = transferMap->begin(); it != transferMap->end(); ++it) {
        if (it->second->fd > 0 and it->first->fd > 0) {
            (*ntransferPipes)[oldNewMap[it->first]] = oldNewMap[it->second];
            (*ntransferPipes)[oldNewMap[it->second]] = oldNewMap[it->first];
        }
    }


    for (std::map<pollfd*, std::vector<char> >::iterator it = dataPieces->begin(); it != dataPieces->end(); ++it) {
        if (it->first->fd > 0)
            (*ndataPieces)[oldNewMap[it->first]] = it->second;
    }

    std::map<pollfd*, typeConnectionAndPath> newDescsToPath;
    for (std::map<pollfd*, typeConnectionAndPath>::iterator it = descsToPath.begin(); it != descsToPath.end(); ++it) {
        if (it->first->fd > 0) {
            newDescsToPath[oldNewMap[it->first]] = it->second;
        }
    }
    std::map<pollfd*, std::string> newCacheWaits;
    for (std::map<pollfd*, std::string>::iterator it = cacheWaits.begin(); it != cacheWaits.end(); ++it) {
        if (it->first->fd > 0) {
            newCacheWaits[oldNewMap[it->first]] = it->second;
        }
    }
    cacheWaits.swap(newCacheWaits);

    descsToPath.swap(newDescsToPath);

    dataPieces->swap(*ndataPieces);
    delete ndataPieces;

    transferMap->swap(*ntransferPipes);
    delete (ntransferPipes);

    pollDescryptors->swap(*npollDescryptors);
    delete npollDescryptors;


}

void CacheProxy::init(int port) {

    signal(SIGPIPE, SIG_IGN);
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    pollDescryptors = new std::vector<pollfd>;
    transferMap = new std::map<pollfd*, pollfd*>;
    dataPieces = new std::map<pollfd*, std::vector<char> >;


    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (serverSocket == -1)
        throw std::runtime_error("Can't open server socket!");

    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
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

void CacheProxy::clearCacheWaits(const std::string &path) {
    std::map<pollfd*, std::string> newCW;
    for (std::map<pollfd*, std::string>::iterator it = cacheWaits.begin(); it != cacheWaits.end(); ++it) {
        if (it->second != path)
            newCW[it->first] = it->second;
    }
    cacheWaits.swap(newCW);
}


#pragma clang diagnostic pop
