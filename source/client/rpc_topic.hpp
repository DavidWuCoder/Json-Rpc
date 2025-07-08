#include <boost/container_hash/detail/float_functions.hpp>
#include <memory>
#include <unordered_set>

#include "requestor.hpp"

namespace wylrpc {
namespace client {
class TopicManager {
public:
    using TopicCallback = std::function<void(const std::string &topic_name,
                                             const std::string &msg)>;
    using ptr = std::shared_ptr<TopicManager>;
    TopicManager(const Requestor::ptr &requestor) : _requestor(requestor) {}

    bool create(const BaseConnection::ptr &conn,
                const std::string &topic_name) {
        return commonRequest(conn, topic_name, TopicOptype::TOPIC_CREATE);
    }
    bool remove(const BaseConnection::ptr &conn,
                const std::string &topic_name) {
        return commonRequest(conn, topic_name, TopicOptype::TOPIC_REMOVE);
    }
    bool subscribe(const BaseConnection::ptr &conn,
                   const std::string &topic_name, const TopicCallback &cb) {
        appendSubscribe(topic_name, cb);
        bool ret =
            commonRequest(conn, topic_name, TopicOptype::TOPIC_SUBSCRIBE);
        if (ret == false) {
            delSubscribe(topic_name);
            return false;
        }
        return true;
    }
    bool cancel(const BaseConnection::ptr &conn,
                const std::string &topic_name) {
        delSubscribe(topic_name);
        return commonRequest(conn, topic_name, TopicOptype::TOPIC_CANCEL);
    }
    bool publish(const BaseConnection::ptr &conn, const std::string &topic_name,
                 const std::string &msg) {
        return commonRequest(conn, topic_name, TopicOptype::TOPIC_PUBLISH, msg);
    }

    void onPublish(const BaseConnection::ptr &conn,
                   const TopicRequest::ptr &msg) {
        // 1.先判断操作类型，如果不是发布则不用处理
        if (msg->optype() != TopicOptype::TOPIC_PUBLISH) {
            ELOG("收到错误类型消息，期待类型：主题发布");
            return;
        }
        // 2.取出主题名称和消息内容
        std::string topic_name = msg->topic();
        std::string topic_msg = msg->topicMsg();
        // 3.查找对应的回调函数，有则处理，否则报错
        auto callback = getSubscribe(topic_name);
        if (!callback) {
            ELOG("%s 主题没有回调函数，无法处理", topic_name.c_str());
            return;
        }
        return callback(topic_name, topic_msg);
    }

private:
    void appendSubscribe(const std::string &topic_name,
                         const TopicCallback &cb) {
        std::lock_guard<std::mutex> guard(_mutex);
        _topic_callbacks.insert(std::make_pair(topic_name, cb));
    }
    void delSubscribe(const std::string &topic_name) {
        std::lock_guard<std::mutex> guard(_mutex);
        _topic_callbacks.erase(topic_name);
    }
    const TopicCallback getSubscribe(const std::string &topic_name) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _topic_callbacks.find(topic_name);
        if (it == _topic_callbacks.end()) {
            return TopicCallback();
        }
        return it->second;
    }
    bool commonRequest(const BaseConnection::ptr &conn,
                       const std::string &topic_name, const TopicOptype optype,
                       const std::string &msg = "") {
        // 构建消息请求
        auto req_msg = MessageFactory::create<TopicRequest>();
        req_msg->setId(UUID::uuid());
        req_msg->setMType(MType::REQ_TOPIC);
        req_msg->setTopicKey(topic_name);
        req_msg->setOptype(optype);
        if (optype == TopicOptype::TOPIC_PUBLISH) {
            req_msg->setTopicMsg(msg);
        }
        // 2.发送请求
        BaseMessage::ptr rsp_msg;
        bool ret = _requestor->send(conn, req_msg, rsp_msg);
        if (ret == false) {
            ELOG("主题操作请求失败");
            return false;
        }
        // 3.等待响应
        DLOG("收到响应, 等待处理");
        auto topic_rsp_msg = std::dynamic_pointer_cast<TopicResponse>(rsp_msg);
        if (!topic_rsp_msg) {
            ELOG("主题响应类型转换失败");
            return false;
        }
        if (topic_rsp_msg->rcode() != RCode::RCODE_OK) {
            ELOG("主题请求处理出错，%s ",
                 errReason(topic_rsp_msg->rcode()).c_str());
            return false;
        }
        return true;
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, TopicCallback> _topic_callbacks;
    Requestor::ptr _requestor;
};
}  // namespace client
}  // namespace wylrpc
