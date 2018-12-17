#pragma once

#include <string>
#include <map>
#include <vector>
#include <poll.h>
#include <map>


struct RequestInfo {
    std::string method;
    std::string path;
    std::string host;
    std::map<std::string, std::string> otherHeaders;
};

struct typeConnectionAndPath {
    bool isClient;
    std::string path;
};

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
