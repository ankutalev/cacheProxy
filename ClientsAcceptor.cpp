#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <stdexcept>
#include "picohttpparser/picohttpparser.h"
#include "ClientsAcceptor.h"

ClientsAcceptor::ClientsAcceptor() {
    port = DEFAULT_PORT;
    CLIENT_SOCKET_SIZE = sizeof(clientAddr);
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
        throw std::runtime_error("Can't open server socket!");

    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr *) &serverAddr, sizeof(serverAddr)))
        throw std::runtime_error("Can't bind server socket!");


    if (listen(serverSocket, MAXIMIUM_CLIENTS))
        throw std::runtime_error("Can't listen this socket!");

}

ClientsAcceptor::ClientsAcceptor(int port):port(port) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    CLIENT_SOCKET_SIZE = sizeof(clientAddr);
    if (serverSocket == -1)
        throw std::runtime_error("Can't open server socket!");

    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr *) &serverAddr, sizeof(serverAddr)))
        throw std::runtime_error("Can't bind server socket!");


    if (listen(serverSocket,MAXIMIUM_CLIENTS))
        throw std::runtime_error("Can't listen this socket!");



}

bool ClientsAcceptor::listenAndRegister() {
    size_t clientSize = sizeof(sockaddr_in);
    std::string record;
    while (1) {

        clientSocket = accept(serverSocket, (sockaddr *) &clientAddr, (socklen_t *) &clientSize);
        std::cout << "I accept: " << clientSocket << "\n";
        while (ssize_t read = recv(clientSocket, buffer, 100, 0)) {
            record += buffer;
            if (read != 100)
                break;
        }
        std::cout << "Raw http header: \n" << record;

//        char buf[4096];
        const char *path;
        const char *method;
        int pret, minor_version;
        struct phr_header headers[100];
        size_t prevbuflen = 0, method_len, path_len, num_headers;

        num_headers = sizeof(headers) / sizeof(headers[0]);
        pret = phr_parse_request(record.c_str(), record.size(), &method, &method_len, &path, &path_len,
                                 &minor_version, headers, &num_headers, prevbuflen);
//        if (pret > 0)
//            break; /* successfully parsed the request */
        if (pret == -1)
            return -1;
        /* request is incomplete, continue the loop */

        std::cout << "\n\n\nSTART OF INFORMATION\n\n\n";
        printf("request is %d bytes long\n", pret);
        printf("method is %.*s\n", (int) method_len, method);
        printf("path is %.*s\n", (int) path_len, path);
        printf("HTTP version is 1.%d\n", minor_version);
        printf("headers:\n");
        for (int i = 0; i != num_headers; ++i) {
            printf("%.*s: %.*s\n", (int) headers[i].name_len, headers[i].name,
                   (int) headers[i].value_len, headers[i].value);
        }
        std::cout << "\n\n\nEND OF INFORMATION\n\n\n";

        close(clientSocket);
    }
    //    std::cout<< recv(clientSocket,buffer,100,0) << std::cout;
    return true;
}


ClientsAcceptor::~ClientsAcceptor() {
    close(serverSocket);
}
