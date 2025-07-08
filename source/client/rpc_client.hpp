#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "../common/dispatcher.hpp"
#include "../common/message.hpp"
#include "../common/net.hpp"
#include "requestor.hpp"
#include "rpc_caller.hpp"
#include "rpc_registry.hpp"
#include "rpc_topic.hpp"

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
        _client = ClientFactory::create(ip, port);
        _client->setMessageCallback(
            std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
        _client->connect();
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
    DiscoveryClient(const std::string &ip, int port,
                    const Discoverer::OfflineCallback &cb)
        : _requestor(std::make_shared<Requestor>()),
          _discoverer(std::make_shared<Discoverer>(_requestor, cb)),
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
        _client = ClientFactory::create(ip, port);
        _client->setMessageCallback(
            std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
        _client->connect();
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
    RpcClient(bool enableDiscovery, const std::string &ip, int port)
        : _enableDiscovery(enableDiscovery),
          _requestor(std::make_shared<Requestor>()),
          _caller(std::make_shared<RpcCaller>(_requestor)),
          _dispatcher(std::make_shared<Dispatcher>()) {
        // 针对Rpc响应进行回调处理
        DLOG("设置相关回调属性");
        auto rsp_cb =
            std::bind(&client::Requestor::onResponse, _requestor.get(),
                      std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<BaseMessage>(MType::RSP_RPC, rsp_cb);

        // 根据是否启用服务发现进行分类
        if (_enableDiscovery == true) {
            auto offline_cb =
                std::bind(&RpcClient::delClient, this, std::placeholders::_1);
            _dicovery_client =
                std::make_shared<DiscoveryClient>(ip, port, offline_cb);
        } else {
            _rpc_client = ClientFactory::create(ip, port);
            _rpc_client->setMessageCallback(
                std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                          std::placeholders::_1, std::placeholders::_2));
            _rpc_client->connect();
        }
        DLOG("回调属性设置完成");
    }
    // 向外提供的服务调用的接口
    bool call(const std::string &method, const Json::Value &params,
              Json::Value &result) {
        // 获取服务发现者
        BaseClient::ptr client = getClient(method);
        if (client.get() == nullptr) {
            return false;
        }
        // 3.发送请求
        _caller->call(client->connection(), method, params, result);
        return true;
    }
    bool call(const std::string &method, const Json::Value &params,
              RpcCaller::JsonAsyncReponse &result) {
        // 获取服务发现者
        BaseClient::ptr client = getClient(method);
        if (client.get() == nullptr) {
            return false;
        }
        // 3.发送请求
        _caller->call(client->connection(), method, params, result);
        return true;
    }
    bool call(const std::string &method, const Json::Value &params,
              const RpcCaller::JsonResponseCallback &cb) {
        // 获取服务发现者
        BaseClient::ptr client = getClient(method);
        if (client.get() == nullptr) {
            return false;
        }
        // 3.发送请求
        _caller->call(client->connection(), method, params, cb);
        return true;
    }

private:
    BaseClient::ptr newClient(const Address &host) {
        auto client = ClientFactory::create(host.first, host.second);
        client->setMessageCallback(
            std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
        client->connect();
        PutClient(host, client);
        return client;
    }
    BaseClient::ptr getClient(const Address &host) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _rpc_clients.find(host);
        if (it == _rpc_clients.end()) {
            return BaseClient::ptr();
        }
        return it->second;
    }
    BaseClient::ptr getClient(const std::string &method) {
        BaseClient::ptr client;
        if (_enableDiscovery) {
            // 进行服务发现
            Address host;
            bool ret = _dicovery_client->serviceDiscovery(method, host);
            if (ret == false) {
                ELOG("%s 服务当前没有可提供服务的主机", method.c_str());
                return BaseClient::ptr();
            }
            DLOG("找到服务主机 %s : %d", host.first.c_str(), host.second);
            // 查看是否已有客户端，直接使用或者创建
            client = getClient(host);
            if (client.get() == nullptr) {
                client = newClient(host);
            }
        } else {
            client = _rpc_client;
        }
        return client;
    }
    void PutClient(const Address &host, const BaseClient::ptr &client) {
        std::lock_guard<std::mutex> guard(_mutex);
        _rpc_clients.insert(std::make_pair(host, client));
    }
    void delClient(const Address &host) {
        std::lock_guard<std::mutex> guard(_mutex);
        _rpc_clients.erase(host);
    }

private:
    struct AddressHash {
        size_t operator()(const Address &host) const {
            std::string addr = host.first + std::to_string(host.second);
            return std::hash<std::string>{}(addr);
        }
    };
    bool _enableDiscovery;
    DiscoveryClient::ptr _dicovery_client;
    Requestor::ptr _requestor;
    RpcCaller::ptr _caller;
    Dispatcher::ptr _dispatcher;
    BaseClient::ptr _rpc_client;
    std::mutex _mutex;
    std::unordered_map<Address, BaseClient::ptr, AddressHash> _rpc_clients;
};

class TopicClient {
public:
    using ptr = std::shared_ptr<RegistryClient>;
    // 构造函数传入注册中心地址，用于连接注册中心
    TopicClient(const std::string &ip, int port)
        : _requestor(std::make_shared<Requestor>()),
          _topic_manager(std::make_shared<TopicManager>(_requestor)),
          _dispatcher(std::make_shared<Dispatcher>()) {
        _dispatcher->registerHandler<BaseMessage>(
            MType::RSP_TOPIC,
            std::bind(&client::Requestor::onResponse, _requestor.get(),
                      std::placeholders::_1, std::placeholders::_2));
        _dispatcher->registerHandler<TopicRequest>(
            MType::REQ_TOPIC,
            std::bind(&TopicManager::onPublish, _topic_manager.get(),
                      std::placeholders::_1, std::placeholders::_2));

        _client = ClientFactory::create(ip, port);
        _client->setMessageCallback(
            std::bind(&Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
        _client->connect();
    }
    bool create(const std::string &topic_name) {
        return _topic_manager->create(_client->connection(), topic_name);
    }
    bool remove(const std::string &topic_name) {
        return _topic_manager->remove(_client->connection(), topic_name);
    }
    bool subscribe(const std::string &topic_name,
                   const TopicManager::TopicCallback &cb) {
        return _topic_manager->subscribe(_client->connection(), topic_name, cb);
    }
    bool cancel(const std::string &topic_name) {
        return _topic_manager->cancel(_client->connection(), topic_name);
    }
    bool publish(const std::string &topic_name, const std::string &msg) {
        return _topic_manager->publish(_client->connection(), topic_name, msg);
    }
    void shutdown() { _client->shutdown(); }

private:
    Requestor::ptr _requestor;
    TopicManager::ptr _topic_manager;
    Dispatcher::ptr _dispatcher;
    BaseClient::ptr _client;
};
}  // namespace client
}  // namespace wylrpc
