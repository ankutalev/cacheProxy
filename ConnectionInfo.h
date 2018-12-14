#pragma once

#include <string>
#include <map>


struct ConnectionInfo {
    std::string method;
    std::string path;
    std::string host;
    std::map<std::string, std::string> otherHeaders;
};

struct typeConnectionAndPath {
    bool isClient;
    std::string path;
};
