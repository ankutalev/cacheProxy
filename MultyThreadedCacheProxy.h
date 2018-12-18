#pragma once

#include <netinet/in.h>
#include <map>
#include <string>
#include <vector>

class MultyThreadedCacheProxy {
public:
    MultyThreadedCacheProxy();

    ~MultyThreadedCacheProxy();

    explicit MultyThreadedCacheProxy(int port);

    void startWorking();

private:
    void init(int port);

private:
    int serverSocket;
    const static int MAXIMIUM_CLIENTS = 2048;
    const static int DEFAULT_PORT = 8081;
    sockaddr_in serverAddr;
    std::map<std::string, std::vector<char> > cache;
    std::map<std::string, bool> cacheLoaded;
    pthread_mutex_t loadedMutex;
    pthread_cond_t cv;
};
