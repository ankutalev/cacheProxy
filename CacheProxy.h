#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "RequestInfo.h"


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

    void registerForWrite(pollfd* fd) { fd->events = POLLOUT; }

    void targetConnect(std::vector<pollfd>::iterator* clientIterator);

    void sendData(std::vector<pollfd>::iterator* clientIterator);

    void writeToClient(std::vector<pollfd>::iterator* clientIterator);

    void acceptConnection(pollfd* client);

    void readFromServer(std::vector<pollfd>::iterator* clientIterator);

    void clearCacheWaits(const std::string &path);
private:
    int port;
    int serverSocket;
    const static int MAXIMIUM_CLIENTS = 2048;
    const static int DEFAULT_PORT = 8080;
    const static int POLL_DELAY = 3000;
    const static int BUFFER_LENGTH = 5000;
    char buffer[BUFFER_LENGTH];
    sockaddr_in serverAddr;
    std::map<std::string, std::vector<char> > cache;
    std::map<std::string, bool> cacheLoaded;
    std::vector<pollfd>* pollDescryptors;
    std::map<pollfd*, int> lastSendingPositionFromCache;
    std::map<pollfd*, pollfd*>* transferMap;
    std::map<pollfd*, typeConnectionAndPath> descsToPath;
    std::map<pollfd*, std::vector<char> >* dataPieces;
    std::map<pollfd*, std::string> cacheWaits;
};


