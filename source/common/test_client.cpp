#include <thread>

#include "abstract.hpp"
#include "dispatcher.hpp"
#include "fields.hpp"
#include "message.hpp"
#include "net.hpp"

void onRpcResponse(const wylrpc::BaseConnection::ptr &conn,
                   const wylrpc::RpcResponse::ptr &msg) {
    ELOG("收到RPC响应")
    std::string body = msg->serialize();
    std::cout << body << std::endl;
}
void onTopicResponse(const wylrpc::BaseConnection::ptr &conn,
                     const wylrpc::TopicResponse::ptr &msg) {
    ELOG("收到TOPIC响应")
    std::string body = msg->serialize();
    std::cout << body << std::endl;
}

int main() {
    auto dispatcher = std::make_shared<wylrpc::Dispatcher>();
    dispatcher->registerHandler<wylrpc::RpcResponse>(wylrpc::MType::RSP_RPC,
                                                     onRpcResponse);
    dispatcher->registerHandler<wylrpc::TopicResponse>(wylrpc::MType::RSP_TOPIC,
                                                       onTopicResponse);

    auto client = wylrpc::ClientFactory::create("127.0.0.1", 8080);
    client->setMessageCallback(
        std::bind(&wylrpc::Dispatcher::onMessage, dispatcher.get(),
                  std::placeholders::_1, std::placeholders::_2));
    client->connect();

    auto rpc_req = wylrpc::MessageFactory::create<wylrpc::RpcRequest>();
    rpc_req->setId("11111");
    rpc_req->setMType(wylrpc::MType::REQ_RPC);
    rpc_req->setMethod("Add");
    Json::Value param;
    param["num1"] = 11;
    param["num2"] = 22;
    rpc_req->setParams(param);
    client->send(rpc_req);

    DLOG("初始化rpc_topic")
    auto rpc_topic = wylrpc::MessageFactory::create<wylrpc::TopicRequest>();
    rpc_topic->setId("22222");
    rpc_topic->setMType(wylrpc::MType::REQ_TOPIC);
    rpc_topic->setOptype(wylrpc::TopicOptype::TOPIC_PUBLISH);
    rpc_topic->setTopicMsg("hello world");
    client->send(rpc_topic);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    client->shutdown();
    return 0;
}
