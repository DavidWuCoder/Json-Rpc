#include <jsoncpp/json/value.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

#include "../client/requestor.hpp"
#include "../client/rpc_caller.hpp"
#include "../common/dispatcher.hpp"

void callback(const Json::Value& result) { ILOG("result: %d", result.asInt()); }

int main() {
    auto requestor = std::make_shared<wylrpc::client::Requestor>();
    auto caller = std::make_shared<wylrpc::client::RpcCaller>(requestor);

    auto dispatcher = std::make_shared<wylrpc::Dispatcher>();
    dispatcher->registerHandler<wylrpc::BaseMessage>(
        wylrpc::MType::RSP_RPC,
        std::bind(&wylrpc::client::Requestor::onResponse, requestor.get(),
                  std::placeholders::_1, std::placeholders::_2));

    auto client = wylrpc::ClientFactory::create("127.0.0.1", 8080);
    client->setMessageCallback(
        std::bind(&wylrpc::Dispatcher::onMessage, dispatcher.get(),
                  std::placeholders::_1, std::placeholders::_2));
    client->connect();

    Json::Value params;
    params["num1"] = 11;
    params["num2"] = 22;
    auto conn = client->connection();
    Json::Value result;
    bool ret = caller->call(conn, "Add", params, result);
    if (ret != false) {
        DLOG("-----------------------------");
        std::cout << "result: " << result.asInt() << std::endl;
    }

    params["num1"] = 33;
    params["num2"] = 44;
    wylrpc::client::RpcCaller::JsonAsyncReponse future_res;
    ret = caller->call(conn, "Add", params, future_res);
    if (ret != false) {
        DLOG("-----------------------------");
        result = future_res.get();
        std::cout << "result: " << result.asInt() << std::endl;
    }

    params["num1"] = 55;
    params["num2"] = 66;
    ret = caller->call(conn, "Add", params, callback);

    DLOG("-----------------------------");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    client->shutdown();
    return 0;
}
