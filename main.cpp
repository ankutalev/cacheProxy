#include <iostream>
#include "CacheProxy.h"
#include "ThreadPool.h"
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
    CacheProxy* proxy;
    if (argc < 2) {
        proxy = new CacheProxy;
    } else {
        int port = atoi(argv[1]);
        proxy = new CacheProxy(port);
    }
    proxy->startWorking();
    delete proxy;
    return 0;
}

