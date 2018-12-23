#include <cstdlib>
#include "MultyThreadedCacheProxy.h"

int main(int argc, char* argv[]) {
    MultyThreadedCacheProxy* proxy;
    proxy = argc < 2 ? new MultyThreadedCacheProxy : new MultyThreadedCacheProxy(atoi(argv[1]));
    proxy->startWorking();
}
