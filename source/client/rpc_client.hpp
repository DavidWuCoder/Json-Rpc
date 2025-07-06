#pragma once
#include <memory>

#include "../common/dispatcher.hpp"
#include "../common/message.hpp"
#include "../common/net.hpp"
#include "requestor.hpp"
#include "rpc_caller.hpp"
#include "rpc_registry.hpp"

namespace wylrpc {
namespace client {
class RegistryClient {
public:
    using ptr = std::shared_ptr<RegistryClient>;
    // 构造函数传入注册中心地址，用于连接注册中心
    RegistryClient(const std::string &ip, int port)
        : _requestor(std::make_shared<Requestor>()),
          _provider(std::make_shared<Provider>(_requestor)),
          _dispatcher(std::make_shared<Dispatcher>()) {
        _dispatcher->registerHandler<BaseMessage>(
            MType::RSP_SERVICE,
            std::bind(&client::Requestor::onResponse, _requestor.get(),
                      std::placeholders::_1, std::placeholders::_2));
        auto client = ClientFactory::create(ip, port);
        client->setMessageCallback(
            std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
        client->connect();
    }
    // 向外提供的服务注册的接口
    bool registerMethod(const std::string &method, const Address &host) {
        return _provider->registerMethod(_client->connection(), method, host);
    }

private:
    Requestor::ptr _requestor;
    client::Provider::ptr _provider;
    Dispatcher::ptr _dispatcher;
    BaseClient::ptr _client;
};

class DiscoveryClient {
public:
    using ptr = std::shared_ptr<DiscoveryClient>;
    // 构造函数传入注册中心地址，用于连接注册中心
    DiscoveryClient(const std::string &ip, int port)
        : _requestor(std::make_shared<Requestor>()),
          _discoverer(std::make_shared<Discoverer>(_requestor)),
          _dispatcher(std::make_shared<Dispatcher>()) {
        auto rsp_cb =
            std::bind(&client::Requestor::onResponse, _requestor.get(),
                      std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

        auto req_cb =
            std::bind(&client::Discoverer::onServiceRequest, _discoverer.get(),
                      std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE,
                                                     req_cb);
        auto client = ClientFactory::create(ip, port);
        client->setMessageCallback(
            std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
        client->connect();
    }

    // 向外提供的服务发现的接口
    bool serviceDiscovery(const std::string &method, Address &host) {
        return _discoverer->serviceDiscovery(_client->connection(), method,
                                             host);
    }

private:
    Requestor::ptr _requestor;
    client::Discoverer::ptr _discoverer;
    Dispatcher::ptr _dispatcher;
    BaseClient::ptr _client;
};

class RpcClient {
public:
    using ptr = std::shared_ptr<RpcClient>;
    // enableDiscovery 决定是否启用服务发现，也决定传入的是服务中心的地址，
    // 还是服务提供者的地址
    RpcClient(bool enableDiscovery, const std::string &ip, int port) {}
    // 向外提供的服务调用的接口
    bool call(const std::string &method, const Json::Value &params,
              Json::Value &result);
    bool call(const std::string &method, const Json::Value &params,
              RpcCaller::JsonAsyncReponse &result);
    bool call(const std::string &method, const Json::Value &params,
              const RpcCaller::JsonResponseCallback &cb);

private:
    bool _enableDiscovery;
    DiscoveryClient::ptr _dicovery_client;
    Requestor::ptr _requestor;
    RpcCaller::ptr _caller;
    Dispatcher::ptr _dispatcher;
    BaseClient::ptr _rpc_client;
};
}  // namespace client
}  // namespace wylrpc
