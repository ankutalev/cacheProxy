#include <iostream>
#include "ClientsAcceptor.h"
#include "ThreadPool.h"
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>

int main(int argc, char* argv[]) {
//    char path [] = "lib.ru";
//    addrinfo hints = {0};
//    hints.ai_flags = 0;
//    hints.ai_family = AF_INET;
//    hints.ai_socktype = SOCK_STREAM;
//    hints.ai_protocol = IPPROTO_TCP;
//
//    addrinfo *addr = NULL;
//    getaddrinfo(path, NULL, &hints, &addr);
//
//
//    struct sockaddr_in edichka;
//    int sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
//    edichka = *(sockaddr_in*)(addr->ai_addr);
//    edichka.sin_family = AF_INET;
//    edichka.sin_port = htons(80);
//    std::cout<< connect(sock,(sockaddr*)&edichka, sizeof(edichka));
////    write(0,"a",1);
////    printf("a");
//    char get [] = "GET /PROZA/LIMONOV/edichka.txt_with-big-pictures.html HTTP/1.0\r\n\r\n";
//    char buf [1005];
//        std::cout<<write(sock,get, sizeof(get))<<std::endl;
//        std::cout<<read(sock,buf,sizeof(buf))<<std::endl;
//        std::cout<<buf<<std::endl;
//    ;
////    printf("sdd");
//    int x = 0;
//    do {
//        x = static_cast<int>(read(sock, buf, sizeof(buf)));
//        std::cout<<x<<std::endl;
//        if (x!= 0 ) {
//            std::cout << buf << std::endl;
//            buf[x] = '\0';
//        }
////
//    } while(x >0 );
    ClientsAcceptor acceptor;
    acceptor.listenAndRegister();
    return 0;
}
//a->a->a
//prev i j  next
//prev -> next = j
//i > next = j->next
//j- >next = i

