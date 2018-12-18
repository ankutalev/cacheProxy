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
#include "fcntl.h"

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


static void* workerBody(void* arg) {
    RequiredInfo* info = (RequiredInfo*) arg;
    static const int BUFFER_SIZE = 5000;
    char buffer[BUFFER_SIZE];
    std::fill(buffer, buffer + BUFFER_SIZE, 0);
    ssize_t readed = -1;
    std::string request;
    fcntl(info->fd, F_SETFL, fcntl(info->fd, F_GETFL, 0) | O_NONBLOCK);
    pollfd pollStruct;
    pollStruct.fd = info->fd;
    pollStruct.events = POLLIN;
    pollStruct.revents = 0;
    static const int POLL_DELAY = 5000;
    if (!poll(&pollStruct, 1, POLL_DELAY)) {
        std::cout << "Client not ready too long, closing connection" << std::endl;
        close(info->fd);
        return NULL;
    }
        readed = recv(info->fd, buffer, BUFFER_SIZE - 1, 0);
    if (readed == -1) {
        perror("read");
    } else {
        buffer[readed] = 0;
        request += buffer;
        std::cout << "readed " << readed << std::endl;
    }

    RequestInfo headers;
    if (!httpParseRequest(request, &headers)) {
        std::cout << "Invalid http request received!" << std::endl;
        close(info->fd);
        return NULL;
    }
    std::cout << "Request's size" << request.size() << std::endl;
    std::cout << "my request is " << request << std::endl;

    if (headers.method != "GET" and headers.method != "HEAD") {
        std::string notSupporting = "HTTP/1.1 405\r\n\r\nAllow: GET\r\n";
        std::cout << "sended " << send(info->fd, notSupporting.c_str(), notSupporting.size(), 0);
        close(info->fd);

        return NULL;
    }

    pthread_mutex_lock(info->loadedMutex);
    unsigned long areCachePageExists = info->cacheLoaded->count(headers.path);
    pthread_mutex_unlock(info->loadedMutex);
    std::cout << "Are cache exists? " << areCachePageExists << std::endl;

    if (areCachePageExists) {
        pthread_mutex_lock(info->loadedMutex);
        while (not(*info->cacheLoaded)[headers.path]) {
            std::cout << "sleepy for " << headers.path << std::endl;
            pthread_cond_wait(info->cv, info->loadedMutex);
        }
        areCachePageExists = info->cacheLoaded->count(headers.path);
        if (areCachePageExists) {
            pthread_mutex_unlock(info->loadedMutex);
		ssize_t total = 0;
		ssize_t left = (*info->cache)[headers.path].size();
	    while(left) {
                      ssize_t s = send(info->fd, &(*info->cache)[headers.path].front()+total,left, 0);
		      if (s==-1)
				continue;
		total+=s;
		left-=s;
		}
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
        send(info->fd, notSupporting.c_str(), notSupporting.size(), 0);
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

    ssize_t sended = 0;
    if ((sended = send(server, request.c_str(), request.size(), 0)) == -1) {
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
            send(info->fd, serverError.c_str(), serverError.size(), 0);
            close(info->fd);
            return NULL;
        }
        default:
        case NoCache:
            pthread_mutex_lock(info->loadedMutex);
            info->cacheLoaded->erase(headers.path);
            pthread_cond_signal(info->cv);
            pthread_mutex_unlock(info->loadedMutex);
	    ssize_t total = 0;
	    ssize_t left = response.size();
             while(left) {
              ssize_t s = send(info->fd, &response[0]+total, left, 0);
                if (s==-1) {
                        continue;
                }
                left-=s;
                total+=s;
	    }           
	    close(info->fd);
            return NULL;
    }
	ssize_t total = 0;
	ssize_t left = (*info->cache)[headers.path].size();
	while(left) {
              ssize_t s = send(info->fd, &(*info->cache)[headers.path].front()+total, left, 0);
		if (s==-1) {
			continue;
		}
		left-=s;
		total+=s;
	}
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
        }
    }
}










