#include <jsoncpp/json/value.h>

#include <memory>

#include "detail.hpp"
#include "fields.hpp"
#include "message.hpp"

int main() {
    // wylrpc::RpcRequest::ptr rrp =
    //     wylrpc::MessageFactory::create<wylrpc::RpcRequest>();
    // Json::Value param;
    // param["num1"] = 11;
    // param["num2"] = 22;
    // rrp->setMethod("Add");
    // rrp->setParams(param);
    // std::string str = rrp->serialize();
    // std::cout << str << std::endl;
    //
    // wylrpc::BaseMessage::ptr bmp =
    //     wylrpc::MessageFactory::create(wylrpc::MType::REQ_RPC);
    // bool ret = bmp->unserialize(str);
    // if (ret == false) {
    //     return -1;
    // }
    // ret = bmp->check();
    // if (ret == false) {
    //     return -1;
    // }
    // wylrpc::RpcRequest::ptr rrp2 =
    //     std::dynamic_pointer_cast<wylrpc::RpcRequest>(bmp);
    // std::cout << rrp2->method() << std::endl;
    // std::cout << rrp2->params()["num1"] << std::endl;
    // std::cout << rrp2->params()["num2"] << std::endl;
    //
    // wylrpc::TopicRequest::ptr trp =
    //     wylrpc::MessageFactory::create<wylrpc::TopicRequest>();
    // trp->setTopicKey("news");
    // trp->setOptype(wylrpc::TopicOptype::TOPIC_PUBLISH);
    // trp->setTopicMsg("Hello world!");
    // std::string str = trp->serialize();
    // std::cout << str << std::endl;
    //
    // wylrpc::BaseMessage::ptr bmp =
    //     wylrpc::MessageFactory::create(wylrpc::MType::REQ_TOPIC);
    // bool ret = bmp->unserialize(str);
    // if (ret == false) {
    //     return -1;
    // }
    // ret = bmp->check();
    // if (ret == false) {
    //     return -1;
    // }
    // wylrpc::TopicRequest::ptr trp2 =
    //     std::dynamic_pointer_cast<wylrpc::TopicRequest>(bmp);
    // std::cout << trp2->topic() << std::endl;
    // std::cout << (int)trp2->optype() << std::endl;
    // std::cout << trp2->topicMsg() << std::endl;
    //
    // wylrpc::ServiceRequest::ptr trp =
    //     wylrpc::MessageFactory::create<wylrpc::ServiceRequest>();
    // trp->setMethod("Add");
    // trp->setOptype(wylrpc::ServiceOptype::SERVICE_ONLINE);
    // trp->setHost(wylrpc::Address("127.0.0.1", 9090));
    // std::string str = trp->serialize();
    // std::cout << str << std::endl;
    //
    // wylrpc::BaseMessage::ptr bmp =
    //     wylrpc::MessageFactory::create(wylrpc::MType::REQ_SERVICE);
    // bool ret = bmp->unserialize(str);
    // if (ret == false) {
    //     return -1;
    // }
    // ret = bmp->check();
    // if (ret == false) {
    //     return -1;
    // }
    // wylrpc::ServiceRequest::ptr trp2 =
    //     std::dynamic_pointer_cast<wylrpc::ServiceRequest>(bmp);
    // std::cout << trp2->method() << std::endl;
    // std::cout << (int)trp2->optype() << std::endl;
    // std::cout << trp2->host().first << std::endl;
    // std::cout << trp2->host().second << std::endl;
    // wylrpc::RpcResponse::ptr trp =
    //     wylrpc::MessageFactory::create<wylrpc::RpcResponse>();
    // trp->setRCode(wylrpc::RCode::RCODE_OK);
    // trp->setResult(33);
    // std::string str = trp->serialize();
    // std::cout << str << std::endl;
    //
    // wylrpc::BaseMessage::ptr bmp =
    //     wylrpc::MessageFactory::create(wylrpc::MType::RSP_RPC);
    // bool ret = bmp->unserialize(str);
    // if (ret == false) {
    //     return -1;
    // }
    // ret = bmp->check();
    // if (ret == false) {
    //     return -1;
    // }
    // wylrpc::RpcResponse::ptr trp2 =
    //     std::dynamic_pointer_cast<wylrpc::RpcResponse>(bmp);
    // std::cout << (int)trp2->rcode() << std::endl;
    // std::cout << trp2->result().asInt() << std::endl;
    // wylrpc::TopicResponse::ptr trp =
    //     wylrpc::MessageFactory::create<wylrpc::TopicResponse>();
    // trp->setRCode(wylrpc::RCode::RCODE_OK);
    // std::string str = trp->serialize();
    // std::cout << str << std::endl;
    //
    // wylrpc::BaseMessage::ptr bmp =
    //     wylrpc::MessageFactory::create(wylrpc::MType::RSP_TOPIC);
    // bool ret = bmp->unserialize(str);
    // if (ret == false) {
    //     return -1;
    // }
    // ret = bmp->check();
    // if (ret == false) {
    //     return -1;
    // }
    // wylrpc::TopicResponse::ptr trp2 =
    //     std::dynamic_pointer_cast<wylrpc::TopicResponse>(bmp);
    // std::cout << (int)trp2->rcode() << std::endl;
    wylrpc::ServiceResponse::ptr trp =
        wylrpc::MessageFactory::create<wylrpc::ServiceResponse>();
    trp->setRCode(wylrpc::RCode::RCODE_OK);
    trp->setOptype(wylrpc::ServiceOptype::SERVICE_DISCOVERY);
    trp->setMethod("Add");
    std::vector<wylrpc::Address> hosts;
    hosts.emplace_back("127.0.0.1", 9090);
    hosts.emplace_back("127.0.0.1", 9091);
    hosts.emplace_back("127.0.0.1", 9092);
    trp->setHosts(hosts);
    std::string str = trp->serialize();
    std::cout << str << std::endl;

    wylrpc::BaseMessage::ptr bmp =
        wylrpc::MessageFactory::create(wylrpc::MType::RSP_SERVICE);
    bool ret = bmp->unserialize(str);
    if (ret == false) {
        return -1;
    }
    ret = bmp->check();
    if (ret == false) {
        return -1;
    }
    wylrpc::ServiceResponse::ptr trp2 =
        std::dynamic_pointer_cast<wylrpc::ServiceResponse>(bmp);
    std::cout << (int)trp2->rcode() << std::endl;
    std::cout << (int)trp2->optype() << std::endl;
    std::cout << trp2->method() << std::endl;
    std::vector<wylrpc::Address> hosts_resp = trp2->hosts();
    for (auto& [ip, port] : hosts_resp) {
        std::cout << ip << " : " << port << std::endl;
    }
    return 0;
}
