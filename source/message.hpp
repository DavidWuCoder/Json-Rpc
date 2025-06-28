// 消息的具体实现，对应BaseMessage
//
// 实现方式：
// BaseMeaage派生出JsonMessage(多了body成员)
// 由JsonMessage派生出JsonRequest和JsonResponse
// 由JsonRequet派生出三种request(RPC Topic Service)
// 由JsonResponse派生出三种response
#pragma once
#include <jsoncpp/json/value.h>
#include <pthread.h>

#include <cinttypes>

#include "abstract.hpp"
#include "detail.hpp"
#include "fields.hpp"

namespace wylrpc {
typedef std::pair<std::string, int> Address;
class JsonMessage : public BaseMessage {
public:
    using ptr = std::shared_ptr<JsonMessage>;
    virtual std::string serialize() override {
        std::string body;
        bool ret = JSON::serialize(_body, body);
        if (ret == false) {
            return std::string();
        }
        return body;
    }
    virtual bool unserialize(const std::string &msg) override {
        return JSON::unserialize(msg, _body);
    }

protected:
    Json::Value _body;
};

// 这里将实现全留给子类
class JsonRequest : public JsonMessage {
public:
    using ptr = std::shared_ptr<JsonRequest>;
};

class JsonResponse : public JsonMessage {
public:
    using ptr = std::shared_ptr<JsonResponse>;
    // 由于大部分响应都只有状态码，因此这里直接实现check等方法了
    virtual bool check() override {
        if (_body[KEY_RCODE].isNull() == true) {
            ELOG("响应中没有响应状态码");
            return false;
        }
        if (_body[KEY_RCODE].isInt() == false) {
            ELOG("响应状态码类型不是整数");
            return false;
        }
        return true;
    }
    virtual RCode rcode() { return (RCode)_body[KEY_RCODE].asInt(); }
    virtual void setRCode(RCode rcode) { _body[KEY_RCODE] = (int)rcode; }
};

class RpcRequest : public JsonRequest {
public:
    using ptr = std::shared_ptr<RpcRequest>;
    virtual bool check() override {
        if (_body[KEY_METHOD].isNull() == true ||
            _body[KEY_METHOD].isString() == false) {
            ELOG("Rpc请求中没有请求方法，或者请求方法类型不是字符串");
            return false;
        }
        if (_body[KEY_PARAMS].isNull() == true ||
            _body[KEY_PARAMS].isObject() == false) {
            ELOG("Rpc请求中没有参数，或者参数类型的类型不是一个Json::Value");
            return false;
        }
        return true;
    }
    std::string method() { return _body[KEY_METHOD].asString(); }
    void setMethod(const std::string &method_name) {
        _body[KEY_METHOD] = method_name;
    }
    Json::Value params() { return _body[KEY_PARAMS]; }
    void setParams(const Json::Value params) { _body[KEY_PARAMS] = params; }
};

class TopicRequest : public JsonRequest {
public:
    using ptr = std::shared_ptr<TopicRequest>;
    virtual bool check() override {
        if (_body[KEY_TOPIC_KEY].isNull() == true ||
            _body[KEY_TOPIC_KEY].isString() == false) {
            ELOG("主题请求中没有主题名称，或者主题名称的类型不是字符串");
            return false;
        }
        if (_body[KEY_OPTYPE].isNull() == true ||
            _body[KEY_OPTYPE].isIntegral() == false) {
            ELOG("主题请求中没有操作类型，或者操作类型不是整数");
            return false;
        }
        if (_body[KEY_OPTYPE].asInt() == (int)TopicOptype::TOPIC_PUBLISH &&
            (_body[KEY_TOPIC_MSG].isNull() == true ||
             _body[KEY_TOPIC_MSG].isString() == false)) {
            ELOG("主题发布请求中没有消息或者消息的类型不是字符串");
            return false;
        }
        return true;
    }
    std::string topic() { return _body[KEY_TOPIC_KEY].asString(); }
    void setTopicKey(const std::string &topic_name) {
        _body[KEY_TOPIC_KEY] = topic_name;
    }
    TopicOptype optype() { return (TopicOptype)_body[KEY_OPTYPE].asInt(); }
    void setOptype(const TopicOptype optype) {
        _body[KEY_OPTYPE] = (int)optype;
    }
    std::string topicMsg() { return _body[KEY_TOPIC_MSG].asString(); }
    void setTopicMsg(const std::string &msg) { _body[KEY_TOPIC_MSG] = msg; }
};

class ServiceRequest : public JsonRequest {
public:
    using ptr = std::shared_ptr<ServiceRequest>;
    virtual bool check() override {
        if (_body[KEY_METHOD].isNull() == true ||
            _body[KEY_METHOD].isString() == false) {
            ELOG("服务请求中没有方法名称或者方法名称的类型不是字符串");
            return false;
        }
        if (_body[KEY_OPTYPE].isNull() == true ||
            _body[KEY_OPTYPE].isIntegral() == false) {
            ELOG("服务请求请求中没有操作类型或者操作类型不是整数");
            return false;
        }
        if (_body[KEY_OPTYPE].asInt() !=
                (int)ServiceOptype::SERVICE_DISCOVERY &&
            (_body[KEY_HOST].isNull() == true ||
             _body[KEY_HOST].isObject() == false ||
             _body[KEY_HOST][KEY_HOST_IP].isNull() == true ||
             _body[KEY_HOST][KEY_HOST_IP].isString() == false ||
             _body[KEY_HOST][KEY_HOST_PORT].isNull() == true ||
             _body[KEY_HOST][KEY_HOST_PORT].isIntegral() == false)) {
            ELOG("服务请求中主机信息错误");
            return false;
        }
        return true;
    }
    std::string method() { return _body[KEY_METHOD].asString(); }
    void setMethod(const std::string &method_name) {
        _body[KEY_METHOD] = method_name;
    }
    ServiceOptype optype() { return (ServiceOptype)_body[KEY_OPTYPE].asInt(); }
    void setOptype(const ServiceOptype optype) {
        _body[KEY_OPTYPE] = (int)optype;
    }
    Address host() {
        Address addr;
        addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
        addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
        return addr;
    }
    void setHost(const Address &host) {
        Json::Value val;
        val[KEY_HOST_IP] = host.first;
        val[KEY_HOST_PORT] = host.second;
        _body[KEY_HOST] = val;
    }
};

class RpcResponse : public JsonResponse {
public:
    using ptr = std::shared_ptr<RpcResponse>;
    virtual bool check() override {
        if (_body[KEY_RCODE].isNull() == true ||
            _body[KEY_RCODE].isIntegral() == false) {
            ELOG("RPC响应中没有响应状态码，或者状态码不是整数!");
            return false;
        }
        if (_body[KEY_RESULT].isNull() == true) {
            ELOG("响应中没有RPC调用结果，或者结果类型不是Json::Value！");
            return false;
        }
        return true;
    }
    Json::Value result() { return _body[KEY_RESULT]; }
    void setResult(const Json::Value &result) { _body[KEY_RESULT] = result; }
};
class TopicResponse : public JsonResponse {
public:
    using ptr = std::shared_ptr<TopicResponse>;
};
class ServiceResponse : public JsonResponse {
public:
    using ptr = std::shared_ptr<ServiceResponse>;
    virtual bool check() override {
        if (_body[KEY_RCODE].isNull() == true ||
            _body[KEY_RCODE].isInt() == false) {
            ELOG("Service响应中没有响应状态码，或者状态码类型不是整数");
            return false;
        }
        if (_body[KEY_OPTYPE].isNull() == true ||
            _body[KEY_OPTYPE].isIntegral() == false) {
            ELOG("响应中没有操作类型，或者操作类型不是字符串！");
            return false;
        }
        if (_body[KEY_OPTYPE].asInt() ==
                (int)ServiceOptype::SERVICE_DISCOVERY &&
            (_body[KEY_METHOD].isNull() == true ||
             _body[KEY_METHOD].isString() == false ||
             _body[KEY_HOST].isNull() == true ||
             _body[KEY_HOST].isArray() == false)) {
            ELOG("服务发现响应中METHOD或HOST相关字段错误");
            return false;
        }
        return true;
    }
    ServiceOptype optype() { return (ServiceOptype)_body[KEY_OPTYPE].asInt(); }
    void setOptype(ServiceOptype type) { _body[KEY_OPTYPE] = (int)type; }
    std::string method() { return _body[KEY_METHOD].asString(); }
    void setMethod(const std::string &method) { _body[KEY_METHOD] = method; }
    std::vector<Address> hosts() {
        std::vector<Address> addrs;
        int sz = _body[KEY_HOST].size();
        for (int i = 0; i < sz; i++) {
            Address addr;
            addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
            addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
            addrs.push_back(addr);
        }
        return addrs;
    }
    void setHosts(std::vector<Address> addrs) {
        for (auto &addr : addrs) {
            Json::Value val;
            val[KEY_HOST_IP] = addr.first;
            val[KEY_HOST_PORT] = addr.second;
            _body[KEY_HOST].append(val);
        }
    }
};

// 实现一个生产消息对象的工厂
class MessageFactory {
public:
    static BaseMessage::ptr create(MType mtype) {
        switch (mtype) {
            case MType::REQ_RPC:
                return std::make_shared<RpcRequest>();
            case MType::RSP_RPC:
                return std::make_shared<RpcResponse>();
            case MType::REQ_TOPIC:
                return std::make_shared<TopicRequest>();
            case MType::RSP_TOPIC:
                return std::make_shared<TopicResponse>();
            case MType::REQ_SERVICE:
                return std::make_shared<ServiceRequest>();
            case MType::RSP_SERVICE:
                return std::make_shared<ServiceResponse>();
        }
        return BaseMessage::ptr();
    }
    template <typename T, typename... Args>
    static std::shared_ptr<T> create(Args &&...args) {
        return std::make_shared<T>(std::forward(args)...);
    }
};
}  // namespace wylrpc
