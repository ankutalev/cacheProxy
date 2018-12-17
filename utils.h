#pragma once

#include "RequestInfo.h"

enum ResponseParseStatus {
    OK, NoCache, Error
};

bool httpParseRequest(std::string &req, RequestInfo* info);

ResponseParseStatus httpParseResponse(const char* response, size_t responseLen);
