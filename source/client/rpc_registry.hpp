#pragma once

#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>

#include "../common/message.hpp"
#include "../common/net.hpp"
#include "requestor.hpp"

namespace wylrpc {
namespace client {
class Provider {
public:
    using ptr = std::shared_ptr<Provider>;
    Provider(const Requestor::ptr requestor) : _requestor(requestor) {}
    bool registerMethod(const BaseConnection::ptr &conn,
                        const std::string &method, const Address &host) {
        auto msg_req = MessageFactory::create<ServiceRequest>();
        msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setOptype(ServiceOptype::SERVICE_REGISTRY);
        msg_req->setMethod(method);
        msg_req->setHost(host);
        BaseMessage::ptr msg_rsp;
        bool ret = _requestor->send(conn, msg_req, msg_rsp);
        if (ret == false) {
            ELOG("%s 服务注册失败", method.c_str());
            return false;
        }
        auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
        if (service_rsp.get() == nullptr) {
            ELOG("service_rsp 向下类型转换失败")
            return false;
        }
        if (service_rsp->rcode() != RCode::RCODE_OK) {
            ELOG("服务注册， 原因:%s", errReason(service_rsp->rcode()).c_str());
            return false;
        }
        return true;
    }

private:
    Requestor::ptr _requestor;
};
class MethodHost {
public:
    using ptr = std::shared_ptr<MethodHost>;
    MethodHost() : _idx(0) {}
    MethodHost(std::vector<Address> hosts)
        : _hosts(hosts.begin(), hosts.end()), _idx(0) {}
    void appendHost(const Address &host) {
        std::lock_guard<std::mutex> guard(_mutex);
        _hosts.push_back(host);
    }
    void removeHost(const Address &host) {
        std::lock_guard<std::mutex> guard(_mutex);
        for (auto it = _hosts.begin(); it != _hosts.end(); it++) {
            if (*it == host) {
                _hosts.erase(it);
                break;
            }
        }
    }
    Address chooseHost() {
        std::lock_guard<std::mutex> guard(_mutex);
        size_t pos = _idx++ % _hosts.size();
        return _hosts[pos];
    }
    bool empty() {
        std::lock_guard<std::mutex> guard(_mutex);
        return _hosts.empty();
    }

private:
    std::mutex _mutex;
    size_t _idx;
    std::vector<Address> _hosts;
};

class Discoverer {
public:
    using OfflineCallback = std::function<void(const Address &)>;
    using ptr = std::shared_ptr<Discoverer>;
    Discoverer(const Requestor::ptr requestor, const OfflineCallback &cb)
        : _requestor(requestor), _offline_callback(cb) {}
    bool serviceDiscovery(const BaseConnection::ptr &conn,
                          const std::string &method, Address &host) {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            // 如果找到了服务，直接返回，否则进行服务发现
            auto it = _method_hosts.find(method);
            if (it != _method_hosts.end()) {
                if (!it->second->empty()) {
                    host = it->second->chooseHost();
                    return true;
                }
            }
        }
        // 需要进行服务发现
        auto msg_req = MessageFactory::create<ServiceRequest>();
        msg_req->setId(UUID::uuid());
        msg_req->setMType(MType::REQ_SERVICE);
        msg_req->setOptype(ServiceOptype::SERVICE_DISCOVERY);
        msg_req->setMethod(method);
        BaseMessage::ptr msg_rsp;
        bool ret = _requestor->send(conn, msg_req, msg_rsp);
        DLOG("发送一个服务发现的请求完毕");
        if (ret == false) {
            ELOG("%s 服务注册失败", method.c_str());
            return false;
        }
        auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
        if (service_rsp.get() == nullptr) {
            ELOG("service_rsp 向下类型转换失败")
            return false;
        }
        if (service_rsp->rcode() != RCode::RCODE_OK) {
            ELOG("服务发现， 原因:%s", errReason(service_rsp->rcode()).c_str());
            return false;
        }
        std::lock_guard<std::mutex> guard(_mutex);
        auto method_host = std::make_shared<MethodHost>(service_rsp->hosts());
        if (method_host->empty()) {
            ELOG("%s 服务发现失败, 没有能提供服务的主机", method.c_str());
            return false;
        }
        host = method_host->chooseHost();
        _method_hosts[method] = method_host;
        return true;
    }

    // 进行服务上线下线处理的回调
    void onServiceRequest(const BaseConnection::ptr &conn,
                          const ServiceRequest::ptr &msg) {
        // 判断是上线请求还是下线请求
        auto optype = msg->optype();
        std::lock_guard<std::mutex> guard(_mutex);
        if (optype == ServiceOptype::SERVICE_ONLINE) {
            // 上线请求，找到对应服务，添加一个主机
            auto it = _method_hosts.find(msg->method());
            if (it == _method_hosts.end()) {
                auto method_host = std::make_shared<MethodHost>();
                method_host->appendHost(msg->host());
                _method_hosts[msg->method()] = method_host;
            } else {
                it->second->appendHost(msg->host());
            }

        } else if (optype == ServiceOptype::SERVICE_OFFLINE) {
            // 3.下线请求，找到对应服务，删除一个主机
            auto it = _method_hosts.find(msg->method());
            if (it == _method_hosts.end()) {
                return;
            }
            it->second->removeHost(msg->host());
            _offline_callback(msg->host());
        }
    }

private:
    OfflineCallback _offline_callback;
    std::mutex _mutex;
    std::unordered_map<std::string, MethodHost::ptr> _method_hosts;
    Requestor::ptr _requestor;
};
}  // namespace client
}  // namespace wylrpc
