#include <iostream>
#include "ClientsAcceptor.h"

ClientsAcceptor::ClientsAcceptor() :ClientsAcceptor(DEFAULT_PORT) {

}

ClientsAcceptor::ClientsAcceptor(int port):port(port) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
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
    std::cout<<"I accept: "<<clientSocket;
    return true;
}