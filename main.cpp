#include <iostream>
#include "ClientsAcceptor.h"
#include "ThreadPool.h"
#include <unistd.h>

void* hi(void* arg) {
    printf("hello from %d\n",pthread_self());
    sleep(2);
    printf("by from %d\n",pthread_self());
    return nullptr;
}
int main(int argc, char* argv[]) {
////    auto s = socket(AF_INET,SOCK_STREAM,0);
////    char buffer [100];
////    struct sockaddr_in server,client;
////    server.sin_family = AF_INET;
////    server.sin_addr.s_addr = inet_addr("127.0.0.1");
////    server.sin_port = htons( 8888 );
////    bind(s,(sockaddr*)&server, sizeof(server));
////    listen(s,5);
////
//////    std::cout << "Hello, World!" << std::endl;
////    perror("hello world!");
////    auto clientSize = sizeof(sockaddr_in);
////    auto clientSock = accept(s,(sockaddr*)&client,(socklen_t *)&clientSize);
//
//    try {
//        ClientsAcceptor clientsAcceptor;
//        clientsAcceptor.listenAndRegister();
//    }
//    catch (std::runtime_error& ex) {
//        perror(ex.what());
//        return -1;
//    }
////    while (auto x = recv(clientSock,buffer,100,0)) {
////        std::cout << buffer<<"\n";
////    }
    ThreadPool threadPool;
    for (int i = 0; i < 35; ++i) {
        threadPool.addJob(hi, (void*)i);
    }
    threadPool.startAll();
    sleep(3);
    for (int j = 15; j < 100 ; ++j) {
        threadPool.addJob(hi,(void*)j);
    }
    sleep(50);
    threadPool.stopAll();
    std::cout<<"all dies!";
    return 0;
}