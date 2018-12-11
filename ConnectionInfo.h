#pragma once

#include <string>
#include <map>

enum CasheState {
    OK = 0, NotReady = 1, NoCashe = 2
};

struct ConnectionInfo {
    std::string method;
    std::string path;
    std::string host;
    std::map<std::string, std::string> otherHeaders;
};
