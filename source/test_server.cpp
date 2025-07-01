#include "abstract.hpp"
#include "fields.hpp"
#include "message.hpp"
#include "net.hpp"

void onMessage(const wylrpc::BaseConnection::ptr &conn,
               wylrpc::BaseMessage::ptr &msg) {
    std::string body = msg->serialize();
    std::cout << body << std::endl;
    auto rpc_req = wylrpc::MessageFactory::create<wylrpc::RpcResponse>();
    rpc_req->setId("11111");
    rpc_req->setMType(wylrpc::MType::RSP_RPC);
    rpc_req->setRCode(wylrpc::RCode::RCODE_OK);
    rpc_req->setResult(33);
    conn->send(rpc_req);
}

int main() {
    auto server = wylrpc::ServerFactory::create(8080);
    server->setMessageCallback(onMessage);
    server->start();
    return 0;
}
