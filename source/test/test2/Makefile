CFLAG=-I ../../../build/release-install-cpp11/include/
LFLAG=-L ../../../build/release-install-cpp11/lib/ -l muduo_net -l muduo_base -lpthread -ljsoncpp
all: rpc_client rpc_server reg_server

rpc_server: rpc_server.cpp
	g++ -g -o $@ $^ $(CFLAG) $(LFLAG)
rpc_client: rpc_client.cpp
	g++  -o $@ $^ $(CFLAG) $(LFLAG)
reg_server: registry_server.cpp
	g++ -g -o $@ $^ $(CFLAG) $(LFLAG)

.PHONY:
clean:
	rm -rf rpc_client rpc_server reg_server

