#include "../../server/rpc_server.hpp"

int main() {
    wylrpc::server::RegistryServer reg_server(9091);
    reg_server.start();
    return 0;
}
