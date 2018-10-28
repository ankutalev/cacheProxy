cmake_minimum_required(VERSION 3.12)
project(cashProxy)

set(CMAKE_CXX_STANDARD 98)
add_definitions("-Werror  -Wall -pedantic")
add_executable(cashProxy main.cpp picohttpparser/picohttpparser.h picohttpparser/picohttpparser.c ClientsAcceptor.cpp RecordManager.cpp ThreadPool.cpp ThreadPool.h RecordManager.h)
target_link_libraries(cashProxy "pthread")
add_executable(client client.cpp)