#include "MultyThreadedCacheProxy.h"
#include <stdexcept>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <cstdio>
#include "utils.h"
#include "RequestInfo.h"

void MultyThreadedCacheProxy::init(int port) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    pthread_mutex_init(&loadedMutex, NULL);
    pthread_cond_init(&cv, NULL);
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

}

MultyThreadedCacheProxy::MultyThreadedCacheProxy() {
    init(DEFAULT_PORT);
}

MultyThreadedCacheProxy::MultyThreadedCacheProxy(int port) {
    init(port);
}

MultyThreadedCacheProxy::~MultyThreadedCacheProxy() {
    pthread_mutex_destroy(&loadedMutex);
    pthread_cond_destroy(&cv);
}


struct RequiredInfo {
    int fd;
    std::map<std::string, std::vector<char> >* cache;
    std::map<std::string, bool>* cacheLoaded;
    pthread_mutex_t* loadedMutex;
    pthread_cond_t* cv;
};

bool sendData(int fd, const char* what, ssize_t dataLen) {
    ssize_t left = dataLen;
    ssize_t sended = 0;
    ssize_t total = 0;
    while (left) {
        sended = send(fd, what + total, left, 0);
        if (sended == -1) {
                return false;
        }
        left -= sended;
        total += sended;
    }
    return true;
}

bool readRequest(int from, std::string &req, RequestInfo* info) {
    static const int BUFFER_SIZE = 5000;
    char buffer[BUFFER_SIZE];
    std::fill(buffer, buffer + BUFFER_SIZE, 0);
    RequestParseStatus res;
    do {
        ssize_t received = recv(from, buffer, BUFFER_SIZE - 1, 0);
        if (received == -1)
            return false;
        buffer[received] = 0;
        req += buffer;
        res = httpParseRequest(req, info);
    } while (res == REQ_NOT_FULL);
    std::cout << req << std::endl;
    return res == REQ_OK;
}

static void* workerBody(void* arg) {
    RequiredInfo* info = (RequiredInfo*) arg;
    static const int BUFFER_SIZE = 5000;
    char buffer[BUFFER_SIZE];
    std::fill(buffer, buffer + BUFFER_SIZE, 0);
    ssize_t readed = -1;
    std::string request;
    RequestInfo headers;

    if (not readRequest(info->fd, request, &headers)) {
        close(info->fd);
        return NULL;
    }
    std::cout << "Request's size" << request.size() << std::endl;
    std::cout << "my request is " << request << std::endl;

    if (headers.method != "GET" and headers.method != "HEAD") {
        std::string notSupporting = "HTTP/1.1 405\r\n\r\nAllow: GET\r\n";
        sendData(info->fd, notSupporting.c_str(), notSupporting.size());
        close(info->fd);
        return NULL;
    }

    pthread_mutex_lock(info->loadedMutex);
    unsigned long areCachePageExists = info->cacheLoaded->count(headers.path);
    pthread_mutex_unlock(info->loadedMutex);
    std::cout << "Are cache exists? " << areCachePageExists << "for " << headers.path << std::endl;

    if (areCachePageExists) {
        pthread_mutex_lock(info->loadedMutex);
        while (not(*info->cacheLoaded)[headers.path]) {
            std::cout << "sleepy for " << headers.path << std::endl;
            pthread_cond_wait(info->cv, info->loadedMutex);
        }
        areCachePageExists = info->cacheLoaded->count(headers.path);
        if (areCachePageExists) {
            pthread_mutex_unlock(info->loadedMutex);
            sendData(info->fd, &(*info->cache)[headers.path].front(), (*info->cache)[headers.path].size());
            close(info->fd);
            return NULL;
        }
        std::cout << "cache ischez = (" << std::endl;
        pthread_mutex_unlock(info->loadedMutex);
    }

    addrinfo hints = {0};
    hints.ai_flags = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addr = NULL;
    getaddrinfo(headers.host.c_str(), NULL, &hints, &addr);
    sockaddr_in targetAddr;

    if (!addr) {
        std::cout << "Can't resolve host!" << std::endl;
        std::string notSupporting = "HTTP/1.1 523\r\n\r\n";
        sendData(info->fd, notSupporting.c_str(), notSupporting.size());
        close(info->fd);
        return NULL;
    }

    targetAddr = *(sockaddr_in*) (addr->ai_addr);
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(80);

    int server = socket(AF_INET, SOCK_STREAM, 0);
    std::cout << "AND MY TARGET IS " << server << std::endl;
    if (server == -1) {
        std::cout << "Can't open target socket! Terminating" << std::endl;
        close(info->fd);
        return NULL;
    }
    if (connect(server, (sockaddr*) &targetAddr, sizeof(targetAddr)) != 0) {
        std::cout << "Can't connect to target! Terminating!" << std::endl;
        close(info->fd);
        return NULL;
    }
    if (!sendData(server, request.c_str(), request.size())) {
        std::cout << "Can't send  request to target! Terminating!" << std::endl;
        close(info->fd);
        close(server);
        return NULL;
    }

    std::fill(buffer, buffer + BUFFER_SIZE, 0);
    std::vector<char> response;

    pthread_mutex_lock(info->loadedMutex);
    (*info->cacheLoaded)[headers.path] = false;
    pthread_mutex_unlock(info->loadedMutex);
    do {
        readed = recv(server, buffer, BUFFER_SIZE - 1, 0);
        buffer[readed] = 0;
        for (int i = 0; i < readed; ++i) {
            response.push_back(buffer[i]);
        }
    } while (readed > 0);
    close(server);
    ResponseParseStatus status = httpParseResponse(&response[0], response.size());


    std::string serverError = "HTTP/1.1 523\r\n\r\n";

    switch (status) {
        case OK:
            pthread_mutex_lock(info->loadedMutex);
            std::cout << "sen cache for " << headers.path << " as ready" << std::endl;
            (*info->cacheLoaded)[headers.path] = true;
            (*info->cache)[headers.path].swap(response);
            pthread_cond_signal(info->cv);
            pthread_mutex_unlock(info->loadedMutex);
            break;
        case Error: {
            std::cout << "CACHE ERASED" << std::endl;
            pthread_mutex_lock(info->loadedMutex);
            info->cacheLoaded->erase(headers.path);
            pthread_cond_signal(info->cv);
            pthread_mutex_unlock(info->loadedMutex);
            sendData(info->fd, serverError.c_str(), serverError.size());
            close(info->fd);
            return NULL;
        }
        default:
        case NoCache:
            pthread_mutex_lock(info->loadedMutex);
            info->cacheLoaded->erase(headers.path);
            std::cout << "cache erazed so due to no cache " << std::endl;
            pthread_cond_signal(info->cv);
            pthread_mutex_unlock(info->loadedMutex);
            sendData(info->fd, &response[0], response.size());
            close(info->fd);
            return NULL;
    }
    sendData(info->fd, &(*info->cache)[headers.path].front(), (*info->cache)[headers.path].size());
    close(info->fd);
    return NULL;
}


void MultyThreadedCacheProxy::startWorking() {
    sockaddr_in clientAddr;
    size_t addrSize = sizeof(clientAddr);
    std::vector<RequiredInfo> infos;
    infos.reserve(MAXIMIUM_CLIENTS);
    while (1) {
        std::cout << "wait for aceptr" << std::endl;
        int client = accept(serverSocket, (sockaddr*) &clientAddr, (socklen_t*) &addrSize);
        pthread_t thread;
        RequiredInfo info;
        std::cout << "I ACCEPT THREAD " << client << std::endl;
        info.loadedMutex = &loadedMutex;
        info.cv = &cv;
        info.cacheLoaded = &cacheLoaded;
        info.cache = &cache;
        info.fd = client;
        infos.push_back(info);
        if (pthread_create(&thread, NULL, workerBody, &*(infos.end() - 1))) {
            std::cerr << "CAN' T CREATE THREAD " << std::endl;
            close(client);
            continue;
        }
        pthread_detach(thread);
    }
}










