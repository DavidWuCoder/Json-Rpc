#include "../../server/rpc_server.hpp"

#include "../../common/detail.hpp"
void Add(const Json::Value &req, Json::Value &rsp) {
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

int main() {
    auto sd_factory = std::make_unique<wylrpc::server::SDFactory>();
    sd_factory->setMethodName("Add");
    sd_factory->setParamDesc("num1", wylrpc::server::VType::INTEGRA);
    sd_factory->setParamDesc("num2", wylrpc::server::VType::INTEGRA);
    sd_factory->setReturnType(wylrpc::server::VType::INTEGRA);
    sd_factory->setCallback(Add);

    wylrpc::server::RpcServer server(wylrpc::Address("127.0.0.1", 8080), true,
                                     wylrpc::Address("127.0.0.1", 9091));
    server.registerMethod(sd_factory->build());
    server.start();
    return 0;
}
