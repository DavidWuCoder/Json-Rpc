#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "../common/message.hpp"
#include "../common/net.hpp"

namespace wylrpc {
namespace server {
class TopicManager {
public:
    using ptr = std::shared_ptr<TopicManager>;

    void onTopicRequest(const BaseConnection::ptr &conn,
                        const TopicRequest::ptr &msg) {
        TopicOptype optype = msg->optype();
        bool ret = true;
        switch (optype) {
            case TopicOptype::TOPIC_CREATE:
                createTopic(msg);
                break;
            case TopicOptype::TOPIC_REMOVE:
                removeTopic(conn, msg);
                break;
            case TopicOptype::TOPIC_SUBSCRIBE:
                ret = subscribeTopic(conn, msg);
                break;
            case TopicOptype::TOPIC_CANCEL:
                cancelTopic(conn, msg);
                break;
            case TopicOptype::TOPIC_PUBLISH:
                ret = publishTopic(conn, msg);
                break;
            default:
                return errResponse(conn, msg, RCode::RCODE_INVALID_OPTYPE);
        }
        if (!ret) return errResponse(conn, msg, RCode::RCODE_NOT_FOUND_TOPIC);
        return topicResponse(conn, msg);
    }

    // 订阅者断开连接需要删除其关联的数据
    void onShutdown(const BaseConnection::ptr &conn) {
        // 1. 先判断是不是订阅者，不是的话直接返回
        std::vector<Topic::ptr> topics;
        Subscriber::ptr subscriber;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _subscribers.find(conn);
            if (it == _subscribers.end()) {
                // 不是一个订阅者，无需任何操作
                return;
            }
            subscriber = it->second;
            // 获取订阅者退出，受到影响的主题对象
            for (auto &topic_name : subscriber->_topics) {
                auto topic_it = _topics.find(topic_name);
                if (topic_it == _topics.end()) {
                    continue;
                }
                topics.push_back(topic_it->second);
            }
            // 4. 删除订阅者信息
            _subscribers.erase(it);
        }
        // 3.在所有影响的主题对象中删除订阅者
        for (auto &topic : topics) {
            topic->removeSubscriber(subscriber);
        }
    }

private:
    void errResponse(const BaseConnection::ptr &conn,
                     const TopicRequest::ptr &msg, RCode rcode) {
        auto msg_rsp = MessageFactory::create<TopicResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_TOPIC);
        msg_rsp->setRCode(rcode);
        conn->send(msg_rsp);
    }
    void topicResponse(const BaseConnection::ptr &conn,
                       const TopicRequest::ptr &msg) {
        auto msg_rsp = MessageFactory::create<TopicResponse>();
        msg_rsp->setId(msg->rid());
        msg_rsp->setMType(MType::RSP_TOPIC);
        msg_rsp->setRCode(RCode::RCODE_OK);
        conn->send(msg_rsp);
    }
    void createTopic(const TopicRequest::ptr &msg) {
        std::lock_guard<std::mutex> guard(_mutex);
        // 创建主题，建立映射关系
        std::string topic_name = msg->topic();
        Topic::ptr topic = std::make_shared<Topic>(topic_name);
        _topics.insert(std::make_pair(topic_name, topic));
    }
    void removeTopic(const BaseConnection::ptr &conn,
                     const TopicRequest::ptr &msg) {
        // 需要先找出当前主题的订阅者
        std::string topic_name = msg->topic();
        std::unordered_set<Subscriber::ptr> subscribers;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _topics.find(topic_name);
            if (it == _topics.end()) {
                ELOG("当前主题没有订阅者");
                return;
            }
            subscribers = it->second->_subcribers;
            _topics.erase(it);
        }
        for (auto &subscriber : subscribers) {
            subscriber->removeTopic(topic_name);
        }
    }
    bool subscribeTopic(const BaseConnection::ptr &conn,
                        const TopicRequest::ptr &msg) {
        // 需要先找出订阅者，为该订阅者增加一个主题
        // 如果找不到则新建一个订阅者
        std::string topic_name = msg->topic();
        Topic::ptr topic;
        Subscriber::ptr subscriber;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto topic_it = _topics.find(topic_name);
            if (topic_it == _topics.end()) {
                ELOG("主题不存在");
                return false;
            }
            topic = topic_it->second;
            auto sub_it = _subscribers.find(conn);
            if (sub_it != _subscribers.end()) {
                subscriber = sub_it->second;
            } else {
                subscriber = std::make_shared<Subscriber>(conn);
                _subscribers.insert(std::make_pair(conn, subscriber));
            }
        }
        topic->appendSubscriber(subscriber);
        subscriber->appendTopic(topic_name);
        return true;
    }
    void cancelTopic(const BaseConnection::ptr &conn,
                     const TopicRequest::ptr &msg) {
        std::string topic_name = msg->topic();
        Topic::ptr topic;
        Subscriber::ptr subscriber;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto topic_it = _topics.find(topic_name);
            if (topic_it != _topics.end()) {
                topic = topic_it->second;
            }
            auto sub_it = _subscribers.find(conn);
            if (sub_it != _subscribers.end()) {
                subscriber = sub_it->second;
            }
        }
        if (subscriber) subscriber->removeTopic(topic_name);
        if (topic && subscriber) topic->removeSubscriber(subscriber);
    }

    bool publishTopic(const BaseConnection::ptr &conn,
                      const TopicRequest::ptr &msg) {
        std::string topic_name = msg->topic();
        Topic::ptr topic;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _topics.find(topic_name);
            if (it == _topics.end()) {
                return false;
            }
            topic = it->second;
        }
        topic->publishMessage(msg);
        return true;
    }

private:
    struct Subscriber {
        using ptr = std::shared_ptr<Subscriber>;
        std::mutex _mutex;
        BaseConnection::ptr _conn;
        std::unordered_set<std::string> _topics;  // 订阅者订阅的所有主题

        Subscriber(const BaseConnection::ptr &conn) : _conn(conn) {}
        // 订阅新主题时调用
        void appendTopic(const std::string &topic_name) {
            std::lock_guard<std::mutex> guard(_mutex);
            _topics.insert(topic_name);
        }
        // 取消订阅或者订阅被删除的时候调用
        void removeTopic(const std::string &topic_name) {
            std::lock_guard<std::mutex> guard(_mutex);
            _topics.erase(topic_name);
        }
    };
    struct Topic {
        using ptr = std::shared_ptr<Topic>;
        std::mutex _mutex;
        std::string _topic_name;
        std::unordered_set<Subscriber::ptr> _subcribers;
        Topic(const std::string &topic_name) : _topic_name(topic_name) {}

        // 订阅主题时调用
        void appendSubscriber(const Subscriber::ptr &subscriber) {
            std::lock_guard<std::mutex> guard(_mutex);
            _subcribers.insert(subscriber);
        }
        // 删除主题或者取消订阅时调用
        void removeSubscriber(const Subscriber::ptr &subscriber) {
            std::lock_guard<std::mutex> guard(_mutex);
            _subcribers.erase(subscriber);
        }
        // 收到主题发布请求的时候调用
        void publishMessage(const BaseMessage::ptr &msg) {
            std::lock_guard<std::mutex> guard(_mutex);
            for (auto &subscriber : _subcribers) {
                subscriber->_conn->send(msg);
            }
        }
    };

private:
    std::mutex _mutex;
    std::unordered_map<std::string, Topic::ptr> _topics;
    std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers;
};
}  // namespace server
}  // namespace wylrpc
