#include <iostream>
#include "ClientsAcceptor.h"
#include "ThreadPool.h"
#include <unistd.h>


int main(int argc, char* argv[]) {
//    auto s = socket(AF_INET,SOCK_STREAM,0);
//    char buffer [100];
//    struct sockaddr_in server,client;
//    server.sin_family = AF_INET;
//    server.sin_addr.s_addr = inet_addr("127.0.0.1");
//    server.sin_port = htons( 8888 );
//    bind(s,(sockaddr*)&server, sizeof(server));
//    listen(s,5);
//
    std::cout << "Hello, World!" << std::endl;
//    perror("hello world!");
//    auto clientSize = sizeof(sockaddr_in);
//    auto clientSock = accept(s,(sockaddr*)&client,(socklen_t *)&clientSize);

//    try {
//        ClientsAcceptor clientsAcceptor;
//        clientsAcceptor.listenAndRegister();
//    }
//    catch (std::runtime_error& ex) {
//        perror(ex.what());
//        return -1;
//    }
    ThreadPool threadPool;
    threadPool.startAll();
//    while (auto x = recv(clientSock,buffer,100,0)) {
//        std::cout << buffer<<"\n";
//    }


    return 0;
}
