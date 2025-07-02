#include "abstract.hpp"
#include "dispatcher.hpp"
#include "fields.hpp"
#include "message.hpp"
#include "net.hpp"

void onRpcRequest(const wylrpc::BaseConnection::ptr &conn,
                  wylrpc::RpcRequest::ptr &msg) {
    DLOG("收到RPC请求: %s", msg->method().c_str());
    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto rpc_req = wylrpc::MessageFactory::create<wylrpc::RpcResponse>();
    rpc_req->setId("11111");
    rpc_req->setMType(wylrpc::MType::RSP_RPC);
    rpc_req->setRCode(wylrpc::RCode::RCODE_OK);
    rpc_req->setResult(33);
    conn->send(rpc_req);
}

void onTopicRequest(const wylrpc::BaseConnection::ptr &conn,
                    wylrpc::TopicRequest::ptr &msg) {
    DLOG("收到Topic请求");
    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto rpc_req = wylrpc::MessageFactory::create<wylrpc::TopicResponse>();
    rpc_req->setId("22222");
    rpc_req->setMType(wylrpc::MType::RSP_TOPIC);
    rpc_req->setRCode(wylrpc::RCode::RCODE_OK);
    conn->send(rpc_req);
}

int main() {
    auto dispatcher = std::make_shared<wylrpc::Dispatcher>();
    dispatcher->registerHandler<wylrpc::RpcRequest>(wylrpc::MType::REQ_RPC,
                                                    onRpcRequest);
    dispatcher->registerHandler<wylrpc::TopicRequest>(wylrpc::MType::REQ_TOPIC,
                                                      onTopicRequest);
    auto server = wylrpc::ServerFactory::create(8080);
    server->setMessageCallback(
        std::bind(&wylrpc::Dispatcher::onMessage, dispatcher.get(),
                  std::placeholders::_1, std::placeholders::_2));
    server->start();
    return 0;
}
