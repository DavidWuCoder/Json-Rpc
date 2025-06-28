#pragma once

#include <muduo/base/CountDownLatch.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>

#include "abstract.hpp"
#include "detail.hpp"
#include "fields.hpp"
#include "message.hpp"

namespace wylrpc {
class MuduoBuffer : public BaseBuffer {
public:
    using ptr = std::shared_ptr<MuduoBuffer>;
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
        return std::make_shared<MuduoBuffer>(std::forward(args)...);
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
        return std::make_shared<LVProtocol>(std::forward(args)...);
    }
};
}  // namespace wylrpc
