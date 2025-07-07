#pragma once

#include <algorithm>
#include <boost/core/enable_if.hpp>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

#include "../common/message.hpp"
#include "../common/net.hpp"

namespace wylrpc {
namespace server {
class ProviderManager {
public:
    using ptr = std::shared_ptr<ProviderManager>;
    struct Provider {
        using ptr = std::shared_ptr<Provider>;
        std::mutex _mutex;
        BaseConnection::ptr _conn;
        Address _host;
        std::vector<std::string> _methods;
        Provider(const BaseConnection::ptr &conn, const Address &host)
            : _conn(conn), _host(host) {}
        void appendMethod(const std::string &method) {
            std::lock_guard<std::mutex> guard(_mutex);
            _methods.emplace_back(method);
        }
    };
    // 服务注册时调用
    void addProvider(const BaseConnection::ptr &conn, const Address &host,
                     const std::string &method) {
        // 先查找连接对应的服务提供者，有则直接用，没有则新建一个提供者
        Provider::ptr provider;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _conn_to_provider.find(conn);
            if (it != _conn_to_provider.end()) {
                provider = it->second;
            } else {
                provider = std::make_shared<Provider>(conn, host);
                _conn_to_provider.insert(std::make_pair(conn, provider));
            }
            // method 方法多了一个provider
            auto &providers = _method_to_providers[method];
            providers.insert(provider);
        }
        // provider内部新增一个method方法
        provider->appendMethod(method);
    }
    // 连接断开时，获取连接对应的服务(用于服务下线的通知)
    Provider::ptr getProvider(const BaseConnection::ptr &conn) {
        Provider::ptr provider;
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _conn_to_provider.find(conn);
        if (it != _conn_to_provider.end()) {
            return it->second;
        }
        return Provider::ptr();
    }
    // 服务下线时删除相关信息
    void delProvider(const BaseConnection::ptr &conn) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _conn_to_provider.find(conn);
        if (it == _conn_to_provider.end()) {
            // 找不到说明对应连接不是服务提供者
            return;
        }
        // 删除_providers和_conns中的信息
        for (auto &method : it->second->_methods) {
            auto &providers = _method_to_providers[method];
            providers.erase(it->second);
        }
        _conn_to_provider.erase(it);
    }
    std::vector<Address> methodHosts(const std::string &method) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _method_to_providers.find(method);
        if (it == _method_to_providers.end()) {
            return std::vector<Address>();
        }
        std::vector<Address> hosts;

        for (auto &provider : it->second) {
            hosts.push_back(provider->_host);
        }
        return hosts;
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, std::set<Provider::ptr>>
        _method_to_providers;
    std::unordered_map<BaseConnection::ptr, Provider::ptr> _conn_to_provider;
};

class DiscovererManager {
public:
    using ptr = std::shared_ptr<DiscovererManager>;
    struct Discoverer {
        using ptr = std::shared_ptr<Discoverer>;
        std::mutex _mutex;
        BaseConnection::ptr _conn;
        std::vector<std::string> _methods;
        Discoverer(const BaseConnection::ptr &conn) : _conn(conn) {}
        void appendMethod(const std::string &method) {
            std::lock_guard<std::mutex> guard(_mutex);
            _methods.emplace_back(method);
        }
    };
    // 服务发现
    void addDiscoverer(const BaseConnection::ptr &conn,
                       const std::string method) {
        Discoverer::ptr discoverer;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _conn_to_discoverer.find(conn);
            if (it != _conn_to_discoverer.end()) {
                discoverer = it->second;
            } else {
                discoverer = std::make_shared<Discoverer>(conn);
                _conn_to_discoverer.insert(std::make_pair(conn, discoverer));
            }
            auto &discoverers = _method_to_discoverers[method];
            discoverers.insert(discoverer);
        }
        discoverer->appendMethod(method);
    }
    // 断开连接时，查找发现者，删除相关信息
    void delDiscoverer(const BaseConnection::ptr &conn) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _conn_to_discoverer.find(conn);
        if (it == _conn_to_discoverer.end()) {
            // 不是一个服务发现者
            return;
        }
        for (auto &method : it->second->_methods) {
            auto &discoverers = _method_to_discoverers[method];
            discoverers.erase(it->second);
        }
        _conn_to_discoverer.erase(it);
    }
    // 服务上线时发起通知
    void onlineNotify(const std::string &method, const Address &host) {
        return notify(method, host, ServiceOptype::SERVICE_ONLINE);
    }
    // 服务下线时发起通知
    void offlineNotify(const std::string &method, const Address &host) {
        return notify(method, host, ServiceOptype::SERVICE_OFFLINE);
    }

private:
    void notify(const std::string &method, const Address &host,
                ServiceOptype optype) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _method_to_discoverers.find(method);
        if (it == _method_to_discoverers.end()) {
            // 当前服务没有发现者
            return;
        }
        auto msg_req = MessageFactory::create<ServiceRequest>();
        msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setOptype(optype);
        msg_req->setMethod(method);
        msg_req->setHost(host);
        for (auto &dicover : it->second) {
            dicover->_conn->send(msg_req);
        }
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, std::set<Discoverer::ptr>>
        _method_to_discoverers;
    std::unordered_map<BaseConnection::ptr, Discoverer::ptr>
        _conn_to_discoverer;
};

// Provider-Discoverer统一管理
class PDManager {
public:
    using ptr = std::shared_ptr<PDManager>;
    PDManager()
        : _provider_manager(std::make_shared<ProviderManager>()),
          _discoverer_manager(std::make_shared<DiscovererManager>()) {}
    void onServiceRequest(const BaseConnection::ptr &conn,
                          const ServiceRequest::ptr &msg) {
        // 服务的注册与发现
        if (msg->optype() == ServiceOptype::SERVICE_REGISTRY) {
            // 注册：新增一个注册者，然后进行上线通知
            ILOG("%s:%d 注册服务: %s", msg->host().first.c_str(),
                 msg->host().second, msg->method().c_str());
            _provider_manager->addProvider(conn, msg->host(), msg->method());
            _discoverer_manager->onlineNotify(msg->method(), msg->host());
            return registryResponse(conn, msg);
        } else if (msg->optype() == ServiceOptype::SERVICE_DISCOVERY) {
            _discoverer_manager->addDiscoverer(conn, msg->method());
            return dicoveryResponse(conn, msg);
        } else {
            ELOG("收到消息但是操作类型错误");
            return errResponse(conn, msg);
        }
    }
    void onConnectionShutdown(const BaseConnection::ptr &conn) {
        auto provider = _provider_manager->getProvider(conn);
        if (provider != nullptr) {
            ILOG("%s:%d 服务下线", provider->_host.first.c_str(),
                 provider->_host.second);
            for (auto &method : provider->_methods) {
                _discoverer_manager->offlineNotify(method, provider->_host);
            }
            _provider_manager->delProvider(conn);
        }
        _discoverer_manager->delDiscoverer(conn);
    }

private:
    void errResponse(const BaseConnection::ptr &conn,
                     const ServiceRequest::ptr &msg) {
        auto msg_rsp = MessageFactory::create<ServiceResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_SERVICE);
        msg_rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
        msg_rsp->setOptype(ServiceOptype::SERVICE_UNKNOW);
        conn->send(msg_rsp);
    }
    void registryResponse(const BaseConnection::ptr &conn,
                          const ServiceRequest::ptr &msg) {
        auto msg_rsp = MessageFactory::create<ServiceResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_SERVICE);
        msg_rsp->setRCode(RCode::RCODE_OK);
        msg_rsp->setOptype(ServiceOptype::SERVICE_REGISTRY);
        conn->send(msg_rsp);
    }
    void dicoveryResponse(const BaseConnection::ptr &conn,
                          const ServiceRequest::ptr &msg) {
        auto msg_rsp = MessageFactory::create<ServiceResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_SERVICE);
        msg_rsp->setOptype(ServiceOptype::SERVICE_REGISTRY);
        std::vector<Address> hosts =
            _provider_manager->methodHosts(msg->method());
        if (hosts.empty()) {
            msg_rsp->setRCode(RCode::RCODE_NOT_FOUND_SERVICE);
            return conn->send(msg_rsp);
        }
        msg_rsp->setRCode(RCode::RCODE_OK);
        msg_rsp->setMethod(msg->method());
        msg_rsp->setHosts(hosts);
        conn->send(msg_rsp);
    }

private:
    std::mutex _mutex;
    ProviderManager::ptr _provider_manager;
    DiscovererManager::ptr _discoverer_manager;
};
}  // namespace server
}  // namespace wylrpc
