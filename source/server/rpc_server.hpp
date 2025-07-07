#include <memory>

#include "../client/rpc_client.hpp"
#include "../common/dispatcher.hpp"
#include "../common/message.hpp"
#include "../common/net.hpp"
#include "rpc_registry.hpp"
#include "rpc_router.hpp"

namespace wylrpc {
namespace server {
// 注册中心的服务端，只需要针对服务的注册与发现进行处理即可
class RegistryServer {
public:
    using ptr = std::shared_ptr<RegistryServer>;
    RegistryServer(int port)
        : _pd_manager(std::make_shared<PDManager>()),
          _dispatcher(std::make_shared<wylrpc::Dispatcher>()) {
        auto req_cb = std::bind(&PDManager::onServiceRequest, _pd_manager.get(),
                                std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE,
                                                     req_cb);

        _server = wylrpc::ServerFactory::create(port);
        _server->setMessageCallback(
            std::bind(&wylrpc::Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
        _server->setCloseCallback(
            std::bind(&RegistryServer::onConnectionShutdown, this,
                      std::placeholders::_1));
    }

    void start() { _server->start(); }

private:
    void onConnectionShutdown(const BaseConnection::ptr &conn) {
        _pd_manager->onConnectionShutdown(conn);
    }
    PDManager::ptr _pd_manager;
    Dispatcher::ptr _dispatcher;
    BaseServer::ptr _server;
};

class RpcServer {
public:
    using ptr = std::shared_ptr<RpcServer>;
    // 注册中心有两套地址，
    // 一套是Rpc服务器对外提供的访问地址(云服务器的访问和见监听地址不同)
    // 另一套是注册中心的地址
    RpcServer(const Address &access_addr, bool enableRegistry = false,
              const Address &register_server_addr = Address())
        : access_addr(access_addr),
          _enableRegistry(enableRegistry),
          _router(std::make_shared<wylrpc::server::RpcRouter>()),
          _dispatcher(std::make_shared<Dispatcher>()) {
        if (_enableRegistry) {
            _reg_client = std::make_shared<client::RegistryClient>(
                register_server_addr.first, register_server_addr.second);
        }
        // 当前服务器是一个rpcserver, 用于处理rpc的请求
        auto rpc_cb = std::bind(&RpcRouter::onRpcRequest, _router.get(),
                                std::placeholders::_1, std::placeholders::_2);
        _dispatcher->registerHandler<RpcRequest>(MType::REQ_RPC, rpc_cb);

        _server = wylrpc::ServerFactory::create(access_addr.second);
        _server->setMessageCallback(
            std::bind(&wylrpc::Dispatcher::onMessage, _dispatcher.get(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void registerMethod(const ServiceDescription::ptr &desc) {
        if (_enableRegistry) {
            _reg_client->registerMethod(desc->method(), access_addr);
        }
        _router->registerHandler(desc);
    }

    void start() {
        ILOG("服务端启动")
        _server->start();
    }

private:
    bool _enableRegistry;
    Address access_addr;
    client::RegistryClient::ptr _reg_client;
    RpcRouter::ptr _router;
    Dispatcher::ptr _dispatcher;
    BaseServer::ptr _server;
};
}  // namespace server
}  // namespace wylrpc
