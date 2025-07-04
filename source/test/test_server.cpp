#include "../common/dispatcher.hpp"
#include "../server/rpc_router.hpp"

void Add(const Json::Value &req, Json::Value &rsp) {
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

int main() {
    auto router = std::make_shared<wylrpc::server::RpcRouter>();
    auto sd_factory = std::make_unique<wylrpc::server::SDFactory>();
    sd_factory->setMethodName("Add");
    sd_factory->setParamDesc("num1", wylrpc::server::VType::INTEGRA);
    sd_factory->setParamDesc("num2", wylrpc::server::VType::INTEGRA);
    sd_factory->setReturnType(wylrpc::server::VType::INTEGRA);
    sd_factory->setCallback(Add);
    router->registerHandler(sd_factory->build());

    auto dispatcher = std::make_shared<wylrpc::Dispatcher>();
    dispatcher->registerHandler<wylrpc::RpcRequest>(
        wylrpc::MType::REQ_RPC,
        std::bind(&wylrpc::server::RpcRouter::onRpcRequest, router.get(),
                  std::placeholders::_1, std::placeholders::_2));
    auto server = wylrpc::ServerFactory::create(8080);
    server->setMessageCallback(
        std::bind(&wylrpc::Dispatcher::onMessage, dispatcher.get(),
                  std::placeholders::_1, std::placeholders::_2));
    server->start();
    return 0;
}
