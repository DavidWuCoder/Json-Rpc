#include <mutex>
#include <unordered_map>

#include "abstract.hpp"
#include "fields.hpp"
#include "message.hpp"
#include "net.hpp"

namespace wylrpc {
class Dispatcher {
public:
    using ptr = std::shared_ptr<Dispatcher>;

    void registerHandler(MType mtype, const MessageCallback& cb) {
        {
            std::unique_lock<std::mutex> guard(_mutex);
            _handlers.insert(std::make_pair(mtype, cb));
        }
    }
    void onMessage(const wylrpc::BaseConnection::ptr& conn,
                   wylrpc::BaseMessage::ptr& msg) {
        {
            std::unique_lock<std::mutex> guard(_mutex);
            auto it = _handlers.find(msg->mtype());
            if (it != _handlers.end()) {
                it->second(conn, msg);
                return;
            }
            ELOG("收到未知类型的消息");
            conn->shutdown();
        }
    }

private:
    std::mutex _mutex;
    std::unordered_map<wylrpc::MType, MessageCallback> _handlers;
};
}  // namespace wylrpc
