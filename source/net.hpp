#pragma once

#include <muduo/base/CountDownLatch.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_map>

#include "abstract.hpp"
#include "detail.hpp"
#include "fields.hpp"
#include "message.hpp"
#include "muduo/net/Callbacks.h"

namespace wylrpc {
class MuduoBuffer : public BaseBuffer {
public:
    using ptr = std::shared_ptr<MuduoBuffer>;
    MuduoBuffer(muduo::net::Buffer *buf) : _buf(buf) {}
    virtual size_t readableSize() override { return _buf->readableBytes(); }
    virtual int32_t peekInt32() override { return _buf->peekInt32(); }
    virtual int32_t readInt32() override { return _buf->readInt32(); }
    virtual void retrieveInt32() override { return _buf->retrieveInt32(); }
    virtual std::string retrieveAsString(size_t len) override {
        return _buf->retrieveAsString(len);
    }

private:
    muduo::net::Buffer *_buf;
};

class BufferFactory {
public:
    template <typename... Args>
    static BaseBuffer::ptr create(Args &&...args) {
        return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
    }
};

class LVProtocol : public BaseProtocol {
public:
    //  |--Len--|--Value--|
    //  |--Len--|--mtype--|--idlen--|--id--|--body--|
    using ptr = std::shared_ptr<BaseProtocol>;
    virtual bool canProcessed(const BaseBuffer::ptr &buf) override {
        if (buf->readableSize() < lenFieldsLength) {
            return false;
        }
        int32_t total_len = buf->peekInt32();
        if (buf->readableSize() < (total_len + lenFieldsLength)) {
            return false;
        }
        return true;
    }
    virtual bool onMessage(const BaseBuffer::ptr &buf,
                           BaseMessage::ptr &msg) override {
        int32_t total_len = buf->readInt32();
        MType mtype = (MType)buf->readInt32();
        int32_t idlen = buf->readInt32();
        std::string id = buf->retrieveAsString(idlen);
        int32_t body_len =
            total_len - idlen - mtypeFieldsLength - idlenFieldsLength;
        std::string body = buf->retrieveAsString(body_len);
        // DLOG("消息内容: %s", body.c_str())
        msg = MessageFactory::create(mtype);
        if (msg.get() == nullptr) {
            ELOG("消息类型错误，构造消息对象失败！");
            return false;
        }
        bool ret = msg->unserialize(body);
        if (ret == false) {
            ELOG("消息正文序列化失败");
            return false;
        }
        msg->setId(id);
        msg->setMType(mtype);
        return true;
    }
    virtual std::string serialize(const BaseMessage::ptr &msg) override {
        //  |--Len--|--mtype--|--idlen--|--id--|--body--|
        std::string body = msg->serialize();
        std::string id = msg->rid();
        size_t idlen = htonl(id.size());
        auto mtype = htonl((int32_t)msg->mtype());
        int32_t h_total_len =
            mtypeFieldsLength + idlenFieldsLength + id.size() + body.size();
        int32_t n_total_len = htonl(h_total_len);
        std::string result;
        result.reserve(h_total_len);
        result.append((char *)&n_total_len, lenFieldsLength);
        result.append((char *)&mtype, mtypeFieldsLength);
        result.append((char *)&idlen, idlenFieldsLength);
        result.append(id);
        result.append(body);
        return result;
    }

private:
    const size_t lenFieldsLength = 4;
    const size_t mtypeFieldsLength = 4;
    const size_t idlenFieldsLength = 4;
};
class ProtocolFactory {
public:
    template <typename... Args>
    static BaseProtocol::ptr create(Args &&...args) {
        return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
    }
};

class MuduoConnection : public BaseConnection {
public:
    using ptr = std::shared_ptr<BaseConnection>;
    MuduoConnection(const muduo::net::TcpConnectionPtr &conn,
                    const BaseProtocol::ptr &protocol)
        : _conn(conn), _protocol(protocol) {}
    virtual void send(const BaseMessage::ptr &msg) override {
        std::string body = _protocol->serialize(msg);
        _conn->send(body);
    }
    virtual void shutdown() override { _conn->shutdown(); }
    virtual bool connected() override { return _conn->connected(); }

private:
    muduo::net::TcpConnectionPtr _conn;
    BaseProtocol::ptr _protocol;
};
class ConnectionFactory {
public:
    template <typename... Args>
    static BaseConnection::ptr create(Args &&...args) {
        return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
    }
};

class MuduoServer : public BaseServer {
public:
    using ptr = std::shared_ptr<MuduoServer>;
    MuduoServer(int port)
        : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port),
                  "DictServer", muduo::net::TcpServer::kReusePort),
          _protocol(ProtocolFactory::create()) {
        // 设置两种回调函数
        _server.setConnectionCallback(
            std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
        _server.setMessageCallback(
            std::bind(&MuduoServer::onMessage, this, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3));
    }
    virtual void start() override {
        _server.setConnectionCallback(
            std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
        _server.setMessageCallback(
            std::bind(&MuduoServer::onMessage, this, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3));
        _server.start();
        _baseloop.loop();
    }

private:
    // 连接和关闭时的回调函数
    void onConnection(const muduo::net::TcpConnectionPtr &conn) {
        if (conn->connected()) {
            std::cout << "连接建立！" << std::endl;
            BaseConnection::ptr muduo_conn =
                ConnectionFactory::create(conn, _protocol);
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _conns.insert(std::make_pair(conn, muduo_conn));
            }
            if (_cb_connection) _cb_connection(muduo_conn);
        } else {
            std::cout << "连接失败！" << std::endl;
            BaseConnection::ptr muduo_conn;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(conn);
                if (it == _conns.end()) {
                    return;
                }
                muduo_conn = it->second;
                _conns.erase(conn);
            }
            if (_cb_close) _cb_close(muduo_conn);
        }
    }
    // 收到消息时的回调函数
    void onMessage(const muduo::net::TcpConnectionPtr &conn,
                   muduo::net::Buffer *buf, muduo::Timestamp) {
        DLOG("连接有数据到来开始处理");
        auto base_buf = BufferFactory::create(buf);
        // std::string body =
        // base_buf->retrieveAsString(base_buf->readableSize()); DLOG("%s",
        // body.c_str());
        while (1) {
            if (_protocol->canProcessed(base_buf) == false) {
                if (base_buf->readableSize() > maxDataSize) {
                    conn->shutdown();
                    ELOG("缓冲区中数据过大！");
                    return;
                }
                break;
            }
            BaseMessage::ptr msg;
            bool ret = _protocol->onMessage(base_buf, msg);
            if (ret == false) {
                conn->shutdown();
                ELOG("缓冲区中数据错误");
                return;
            }
            BaseConnection::ptr base_conn;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(conn);
                if (it == _conns.end()) {
                    conn->shutdown();
                    return;
                }
                base_conn = it->second;
            }
            if (_cb_message) _cb_message(base_conn, msg);
        }
    }

private:
    const size_t maxDataSize = (1 << 16);
    BaseProtocol::ptr _protocol;
    muduo::net::EventLoop _baseloop;
    muduo::net::TcpServer _server;
    std::mutex _mutex;
    std::map<muduo::net::TcpConnectionPtr, BaseConnection::ptr> _conns;
};
class ServerFactory {
public:
    template <typename... Args>
    static MuduoServer::ptr create(Args &&...args) {
        return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
    }
};

class MuduoClient : public BaseClient {
public:
    using ptr = std::shared_ptr<MuduoClient>;
    MuduoClient(const std::string &sip, int sport)
        : _protocol(ProtocolFactory::create()),
          _baseloop(
              _loopthread.startLoop())  // 客户端只能交给一个线程区进行事件循环
          ,
          _downlatch(1),
          _client(_baseloop, muduo::net::InetAddress(sip, sport),
                  "MuduoClient") {}

    virtual void connect() override {
        DLOG("设置回调函数，连接服务器");
        _client.setConnectionCallback(
            std::bind(&MuduoClient::onConnection, this, std::placeholders::_1));
        _client.setMessageCallback(
            std::bind(&MuduoClient::onMessage, this, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3));
        _client.connect();
        _downlatch.wait();
        DLOG("连接服务器成功, %d", connected());
    }
    virtual bool send(const BaseMessage::ptr &message) override {
        DLOG("准备开始发送消息")
        if (connected() == false) {
            ELOG("连接已断开, 发送失败");
            return false;
        }
        _conn->send(message);
        DLOG("发送消息成功");
        return true;
    }
    virtual void shutdown() override { return _client.disconnect(); }
    virtual bool connected() override { return (_conn && _conn->connected()); }
    virtual BaseConnection::ptr connection() override { return _conn; }

private:
    // 连接和关闭时的回调函数
    void onConnection(const muduo::net::TcpConnectionPtr &conn) {
        // DLOG("连接到来");
        if (conn->connected()) {
            std::cout << "连接建立！" << std::endl;
            _conn = ConnectionFactory::create(conn, _protocol);
            _downlatch.countDown();  // downlatch计数--
            // DLOG("是否连接: %d", connected());
        } else {
            std::cout << "连接失败！" << std::endl;
            _conn.reset();
            _downlatch.countDown();
        }
    }
    // 收到消息时的回调函数
    void onMessage(const muduo::net::TcpConnectionPtr &conn,
                   muduo::net::Buffer *buf, muduo::Timestamp) {
        DLOG("连接有数据到来开始处理");
        auto base_buf = BufferFactory::create(buf);
        // std::string body =
        // base_buf->retrieveAsString(base_buf->readableSize()); DLOG("%s",
        // body.c_str());
        while (1) {
            if (_protocol->canProcessed(base_buf) == false) {
                if (base_buf->readableSize() > maxDataSize) {
                    conn->shutdown();
                    ELOG("缓冲区中数据过大！");
                    return;
                }
                break;
            }
            BaseMessage::ptr msg;
            bool ret = _protocol->onMessage(base_buf, msg);
            if (ret == false) {
                conn->shutdown();
                ELOG("缓冲区中数据错误");
                return;
            }
            if (_cb_message) _cb_message(_conn, msg);
        }
    }

private:
    const size_t maxDataSize = (1 << 16);
    BaseProtocol::ptr _protocol;
    BaseConnection::ptr _conn;
    muduo::CountDownLatch _downlatch;
    muduo::net::EventLoopThread _loopthread;
    muduo::net::EventLoop *_baseloop;
    muduo::net::TcpClient _client;
};
class ClientFactory {
public:
    template <typename... Args>
    static MuduoClient::ptr create(Args &&...args) {
        return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
    }
};
}  // namespace wylrpc
