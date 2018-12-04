#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <stdexcept>
#include <netdb.h>
#include <cstring>
#include <poll.h>
#include "picohttpparser/picohttpparser.h"
#include "ClientsAcceptor.h"

static void *writeToBrowser(void *params) {
    pollfd *info = (pollfd *) params;
    std::cerr << "ALLO!!!";
    write(info->fd, "sosi jopu", 9);
    close(info->fd);
    info->revents = POLLMSG;
    return NULL;
}

static void *registerConnection(void *info) {
    const static int BUFFER_LENGTH = 100;
    char buffer[BUFFER_LENGTH + 1];
    std::fill(buffer, buffer + BUFFER_LENGTH + 1, 0);
    ThreadRegisterInfo *trInfo = (ThreadRegisterInfo *) info;
    std::cout << "I accept: " << trInfo->descyptor->fd << "\n";
    std::string record;
    while (ssize_t read = recv(trInfo->descyptor->fd, buffer, BUFFER_LENGTH, 0)) {
        record += buffer;
        std::cout << "I READ " << read << std::endl;
        if (read != BUFFER_LENGTH)
            break;
    }
    std::cout << " zashel!!!\n";
    std::cout << "i read " << record << std::endl;
    trInfo->descyptor->events = POLLOUT;
//    std::cout<<write(trInfo->descyptor->fd,"sosi jopu",9)<<std::endl;

    const char *path;
    const char *method;
    int pret, minor_version;
    struct phr_header headers[100];
    size_t prevbuflen = 0, method_len, path_len, num_headers;
    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_request(record.c_str(), record.size(), &method, &method_len, &path, &path_len,
                             &minor_version, headers, &num_headers, prevbuflen);
    if (pret == -1)
        return (void *) 1;
    std::cout << "\n\n\nSTART OF INFORMATION\n\n\n";
    printf("request is %d bytes long\n", pret);
    ConnectionInfo connectionInfo;
    connectionInfo.method = method;
    connectionInfo.path = path;
    for (int i = 0; i != num_headers; ++i) {
        connectionInfo.otherHeaders[headers[i].name] = headers[i].value;
        if (!std::strcmp(headers[i].name, "Host"))
            connectionInfo.host = headers[i].value;
    }
    return NULL;
}


ClientsAcceptor::ClientsAcceptor() {
    port = DEFAULT_PORT;
    clientSocket = -1;

    CLIENT_SOCKET_SIZE = sizeof(clientAddr);
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (serverSocket == -1)
        throw std::runtime_error("Can't open server socket!");

    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr *) &serverAddr, sizeof(serverAddr))) {
        perror("govno \n");
        throw std::runtime_error("Can't bind server socket!");
    }


    if (listen(serverSocket, MAXIMIUM_CLIENTS))
        throw std::runtime_error("Can't listen this socket!");

}

ClientsAcceptor::ClientsAcceptor(int port) : port(port) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    CLIENT_SOCKET_SIZE = sizeof(clientAddr);
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

bool ClientsAcceptor::listenAndRegister() {
    size_t clientSize = sizeof(sockaddr_in);
    std::string record;
    pool.startAll();
    while (1) {
        if (clients.empty())
            clientSocket = accept(serverSocket, (sockaddr *) &clientAddr, (socklen_t *) &clientSize);
        pollfd c;
        c.fd = clientSocket;
        c.events = POLLIN;
        clients.push_back(c);
        ThreadRegisterInfo info(&clients.back(), &connections);
        poll(clients.data(), clients.size(), 1000);
        for (auto it = clients.begin(); it != clients.end();) {
            if (it->revents == POLLIN) {
                pool.addJob(registerConnection, &info);
            } else if (it->revents == POLLOUT) {
                pool.addJob(writeToBrowser, &clients[i]);
            } else if (it->revents == POLLERR)
                clients.erase(it);
            else
                ++it;
        }

    }
    return true;
}


ClientsAcceptor::~ClientsAcceptor() {
    close(serverSocket);
}

