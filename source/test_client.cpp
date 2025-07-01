#include <thread>

#include "abstract.hpp"
#include "fields.hpp"
#include "message.hpp"
#include "net.hpp"

void onMessage(const wylrpc::BaseConnection::ptr &conn,
               const wylrpc::BaseMessage::ptr &msg) {
    std::string body = msg->serialize();
    std::cout << body << std::endl;
}

int main() {
    auto client = wylrpc::ClientFactory::create("127.0.0.1", 8080);
    client->setMessageCallback(onMessage);
    client->connect();
    auto rpc_req = wylrpc::MessageFactory::create<wylrpc::RpcRequest>();
    rpc_req->setId("11111");
    rpc_req->setMType(wylrpc::MType::REQ_RPC);
    rpc_req->setMethod(
        "    std::this_thread::sleep_for(std::chrono::seconds(10));Add");
    Json::Value param;
    param["num1"] = 11;
    param["num2"] = 22;
    rpc_req->setParams(param);
    // std::this_thread::sleep_for(std::chrono::seconds(10));
    client->send(rpc_req);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    client->shutdown();
    return 0;
}
