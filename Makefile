src = $(wildcard *.cpp)
obj = $(patsubst %.cpp, %.o, $(src))

all: server client

server: server.o wrap.o
	g++ -std=c++11 server.o wrap.o -o server -Wall -g -pthread

client: client.o wrap.o
	g++ client.o wrap.o -o client -Wall -g -pthread

server.o: threadpool.h
	g++ -std=c++11 -g -c -pthread -Wall server.cpp

wrap.o: wrap.h
	g++ -std=c++11 -g -c -Wall wrap.cpp

.PHONY: clean all
clean:
	-rm -rf server client $(obj)
