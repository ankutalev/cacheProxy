#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <poll.h>
#include <set>
#include "ConnectionInfo.h"
#include "ThreadPool.h"

struct ThreadRegisterInfo {
    ThreadRegisterInfo(int* d, pollfd* pd) : server(d), client(pd) {}
    int* server;
    pollfd* client;
};

struct TargetConnectInfo {
    TargetConnectInfo(int* s, std::vector<pollfd>::iterator* it, std::vector<pollfd>* pd,
                      std::map<pollfd*, std::vector<char> >* dp,
                      std::map<pollfd*, pollfd*>* tm,
                      std::map<pollfd*, typeConnectionAndPath>* ss,
                      std::map<std::string, bool>* ci,
                      std::map<std::string, std::vector<char> >* cch) : server(s), clientIterator(it),
                                                                        pollDescryptos(pd),
                                                                        dataPieces(dp),
                                                                        transferMap(tm),
                                                                        descsToPath(ss),
                                                                        cacheLoaded(ci),
                                                                        cache(cch) {}

    int* server;
    std::vector<pollfd>::iterator* clientIterator;
    std::vector<pollfd>* pollDescryptos;
    std::map<pollfd*, std::vector<char> >* dataPieces;
    std::map<pollfd*, pollfd*>* transferMap;
    std::map<pollfd*, typeConnectionAndPath>* descsToPath;
    std::map<std::string, bool>* cacheLoaded;
    std::map<std::string, std::vector<char> >* cache;
};

struct SendDataInfo {
    SendDataInfo(pollfd* tg, std::map<pollfd*, std::vector<char> >* dp, std::vector<pollfd>* pd)
            : target(tg),
              dataPieces(dp),
              pollDescryptors(pd) {}

    pollfd* target;
    std::map<pollfd*, std::vector<char> >* dataPieces;
    std::vector<pollfd>* pollDescryptors;
};

class ClientsAcceptor {
public:
    ClientsAcceptor();

    ~ClientsAcceptor();

    explicit ClientsAcceptor(int port);

    bool listenAndRegister();

private:
    void pollManage();

    void removeFromPoll();

private:
    int port;
    int serverSocket;
    struct sockaddr_in serverAddr, clientAddr;
    const static int MAXIMIUM_CLIENTS = 2048;
    const static int DEFAULT_PORT = 8080;
    const static int POLL_DELAY = 3000;
    ThreadPool pool;
    std::map<std::string, std::vector<char> > cache;
    std::map<std::string, bool> cacheLoaded;
    std::vector<pollfd>* pollDescryptors;
    std::map<pollfd*, pollfd*>* transferMap;
    std::map<pollfd*, typeConnectionAndPath> descsToPath;
    std::map<pollfd*, std::vector<char> >* dataPieces;
};


