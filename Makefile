all:
		g++ *.cpp picohttpparser/picohttpparser.c -o cashProxy -lsocket -lnsl
