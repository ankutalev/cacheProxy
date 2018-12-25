#pragma once

#include "RequestInfo.h"

enum ResponseParseStatus {
    OK, NoCache, Error
};

enum RequestParseStatus {
    REQ_OK, REQ_NOT_FULL, REQ_ERROR
};

RequestParseStatus httpParseRequest(std::string &req, RequestInfo* info);

ResponseParseStatus httpParseResponse(const char* response, size_t responseLen, RequestInfo* info);
