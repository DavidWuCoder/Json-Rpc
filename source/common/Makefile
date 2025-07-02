CFLAG=-I ../build/release-install-cpp11/include/
LFLAG=-L ../build/release-install-cpp11/lib/ -l muduo_net -l muduo_base -lpthread -ljsoncpp
all: client server

server: test_server.cpp
	g++ -o $@ $^ $(CFLAG) $(LFLAG)
client: test_client.cpp
	g++ -o $@ $^ $(CFLAG) $(LFLAG)

.PHONY:
clean:
	rm -rf client server
