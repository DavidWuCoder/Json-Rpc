CFLAG=-I ../../build/release-install-cpp11/include/
LFLAG=-L ../../build/release-install-cpp11/lib/ -l muduo_net -l muduo_base -lpthread
all: client server

server: server.cpp
	g++ -o $@ $^ $(CFLAG) $(LFLAG)
client: client.cpp
	g++ -o $@ $^ $(CFLAG) $(LFLAG)

.PHONY:
clean:
	rm -rf client server