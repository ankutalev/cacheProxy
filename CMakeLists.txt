cmake_minimum_required(VERSION 3.12)
project(cashProxy)

set(CMAKE_CXX_STANDARD 98)
add_definitions("-Wall  -pedantic")
add_executable(cashProxy proxy.cpp picohttpparser/picohttpparser.h picohttpparser/picohttpparser.c CacheProxy.cpp utils.cpp utils.h)
add_executable(mtCacheProxy mtProxy.cpp picohttpparser/picohttpparser.h picohttpparser/picohttpparser.c MultyThreadedCacheProxy.cpp MultyThreadedCacheProxy.h utils.cpp utils.h)

target_link_libraries(cashProxy "pthread")
target_link_libraries(mtCacheProxy "pthread")