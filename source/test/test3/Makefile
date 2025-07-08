CFLAG=-I ../../../build/release-install-cpp11/include/
LFLAG=-L ../../../build/release-install-cpp11/lib/ -l muduo_net -l muduo_base -lpthread -ljsoncpp
all: publish_client server subscribe_client

publish_client: publish_client.cpp
	g++ -g -o $@ $^ $(CFLAG) $(LFLAG)
server: server.cpp
	g++  -o $@ $^ $(CFLAG) $(LFLAG)
subscribe_client: subscribe_client.cpp
	g++ -g -o $@ $^ $(CFLAG) $(LFLAG)

.PHONY:
clean:
	rm -rf publish_client server subscribe_client

