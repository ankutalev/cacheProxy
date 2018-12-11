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
    std::set<pollfd*>* brokenDescryptors;
};

struct TargetConnectInfo {
    TargetConnectInfo(int* s, pollfd* cl, pollfd* tg, std::map<pollfd*, std::vector<char> >* dp,
                      std::map<pollfd*, pollfd*>* tm,
                      std::set<pollfd*>* bd,
                      std::map<pollfd*, std::string>* ss) : server(s), client(cl), target(tg), dataPieces(dp),
                                                            transferMap(tm),
                                                            brokenDescryptors(bd),
                                                            hostToGets(ss) {}

    int* server;
    pollfd* client;
    pollfd* target;
    std::map<pollfd*, std::vector<char> >* dataPieces;
    std::map<pollfd*, pollfd*>* transferMap;
    std::set<pollfd*>* brokenDescryptors;
    std::map<pollfd*, std::string>* hostToGets;
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
    int i;
    int port;
    int serverSocket;
    int clientSocket;
    std::map<int, ConnectionInfo> connections;
    struct sockaddr_in serverAddr, clientAddr;
    int CLIENT_SOCKET_SIZE;
    const static int MAXIMIUM_CLIENTS = 1024;
    const static int DEFAULT_PORT = 8080;
    const static int POLL_DELAY = 3000;
    ThreadPool pool;

    std::map<std::string, std::vector<char> > cashe;
    std::map<std::string, CasheState> isCacheConsistent;
    std::vector<pollfd>* pollDescryptors;
    std::map<pollfd*, pollfd*>* transferMap;
    std::map<pollfd*, std::string> hostsToGets;
    std::map<pollfd*, std::vector<char> >* dataPieces;
    std::set<pollfd*> brokenDescryptors;
};


