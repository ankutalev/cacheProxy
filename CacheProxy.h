#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "RequestInfo.h"
#include "ThreadPool.h"


class CacheProxy {
public:
    CacheProxy();

    ~CacheProxy();

    explicit CacheProxy(int port);

    void startWorking();


private:
    void pollManage();

    void removeDeadDescryptors();

    void init(int port);

private:
    int port;
    int serverSocket;
    const static int MAXIMIUM_CLIENTS = 2048;
    const static int DEFAULT_PORT = 8080;
    const static int POLL_DELAY = 3000;
    sockaddr_in serverAddr;
    ThreadPool pool;
    std::map<std::string, std::vector<char> > cache;
    std::map<std::string, bool> cacheLoaded;
    std::vector<pollfd>* pollDescryptors;
    std::map<pollfd*, pollfd*>* transferMap;
    std::map<pollfd*, typeConnectionAndPath> descsToPath;
    std::map<pollfd*, std::vector<char> >* dataPieces;
};


