#include "utils.h"
#include <iostream>
#include "picohttpparser/picohttpparser.h"

bool httpParseRequest(std::string &req, RequestInfo* info) {
    const char* path;
    const char* method;
    int pret, minor_version;
    struct phr_header headers[100];
    size_t prevbuflen = 0, method_len, path_len, num_headers;
    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_request(req.c_str(), req.size(), &method, &method_len, &path, &path_len,
                             &minor_version, headers, &num_headers, prevbuflen);
    if (pret < -1)
        return false;
    info->method = method;
    info->method.erase(info->method.begin() + method_len, info->method.end());
    info->path = path;
    info->path.erase(info->path.begin() + path_len, info->path.end());
    for (int i = 0; i != num_headers; ++i) {
        std::string headerName = headers[i].name;
        headerName.erase(headerName.begin() + headers[i].name_len, headerName.end());
        if (headerName != "Connection") {
            info->otherHeaders[headerName] = headers[i].value;
            info->otherHeaders[headerName].erase(info->otherHeaders[headerName].begin() + headers[i].value_len,
                                                 info->otherHeaders[headerName].end());
        }
        if (headerName == "Host")
            info->host = info->otherHeaders[headerName];
    }
    req = info->method + " " + info->path + " " + "HTTP/1.0\r\n";
    for (std::map<std::string, std::string>::iterator it = info->otherHeaders.begin();
         it != info->otherHeaders.end(); ++it) {
        req += it->first;
        req += ": ";
        req += it->second;
        req += "\r\n";
    }
    req += "\r\n";
    return true;
}


ResponseParseStatus httpParseResponse(const char* response, size_t responseLen) {
    const char* message;
    int pret, minor_version, status;
    struct phr_header headers[100];
    size_t prevbuflen = 0, message_len, num_headers;
    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_response(response, responseLen, &minor_version, &status, &message, &message_len,
                              headers, &num_headers, prevbuflen);
    if (pret == -1 or minor_version < 0)
        return Error;
    std::cout << "VERSION " << minor_version << " STATUS " << status << std::endl;
    return (status == 200) ? OK : NoCache;
}
