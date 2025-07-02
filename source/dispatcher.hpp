#include <exception>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "abstract.hpp"
#include "fields.hpp"
#include "message.hpp"
#include "net.hpp"

namespace wylrpc {
class Callback {
public:
    using ptr = std::shared_ptr<Callback>;
    virtual void onMessage(const wylrpc::BaseConnection::ptr& conn,
                           wylrpc::BaseMessage::ptr& msg) = 0;
};
template <typename T>
class CallbackT : public Callback {
public:
    using ptr = std::shared_ptr<CallbackT<T>>;
    using MessageCallback = std::function<void(
        const wylrpc::BaseConnection::ptr& conn, std::shared_ptr<T>& msg)>;

    CallbackT(MessageCallback handler) : _handler(handler) {}
    virtual void onMessage(const wylrpc::BaseConnection::ptr& conn,
                           wylrpc::BaseMessage::ptr& msg) override {
        auto type_msg = std::dynamic_pointer_cast<T>(msg);
        _handler(conn, type_msg);
    }

private:
    MessageCallback _handler;
};
class Dispatcher {
public:
    using ptr = std::shared_ptr<Dispatcher>;

    template <typename T>
    void registerHandler(
        MType mtype, const typename CallbackT<T>::MessageCallback& handler) {
        {
            std::unique_lock<std::mutex> guard(_mutex);
            auto cb = std::make_shared<CallbackT<T>>(handler);
            _handlers.insert(std::make_pair(mtype, cb));
        }
    }
    void onMessage(const wylrpc::BaseConnection::ptr& conn,
                   wylrpc::BaseMessage::ptr& msg) {
        {
            std::unique_lock<std::mutex> guard(_mutex);
            auto it = _handlers.find(msg->mtype());
            if (it != _handlers.end()) {
                it->second->onMessage(conn, msg);
                return;
            }
            ELOG("收到未知类型的消息");
            conn->shutdown();
        }
    }

private:
    std::mutex _mutex;
    std::unordered_map<wylrpc::MType, Callback::ptr> _handlers;
};
}  // namespace wylrpc
