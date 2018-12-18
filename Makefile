all: cashProxy mtCacheProxy

cashProxy:
	g++ proxy.cpp CacheProxy.cpp utils.cpp picohttpparser/picohttpparser.c -o cashProxy -lsocket -lnsl
mtCacheProxy:
	g++ mtProxy.cpp MultyThreadedCacheProxy.cpp utils.cpp  picohttpparser/picohttpparser.c -o mtcacheProxy -lsocket -lnsl
