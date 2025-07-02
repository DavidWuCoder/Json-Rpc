#pragma once
#include <jsoncpp/json/value.h>

#include <mutex>
#include <unordered_map>
#include <vector>

#include "../common/message.hpp"
#include "../common/net.hpp"

namespace wylrpc {
namespace server {

// 所需全部类型
enum class VType { BOOL = 0, INTEGRA, NUMERIC, STRING, ARRAY, OBJECT };

// 服务参数描述类
class ServiceDescription {
public:
    using ptr = std::shared_ptr<ServiceDescription>;
    using ServiceCallback =
        std::function<void(const Json::Value &, Json::Value &)>;
    using ParamDescription = std::pair<std::string, VType>;

    ServiceDescription(const std::string &&mname, ServiceCallback &&cb,
                       std::vector<ParamDescription> &&desc, VType rtype)
        : _method_name(std::move(mname)),
          _callback(std::move(cb)),
          _param_desc(std::move(desc)),
          _return_type(rtype)

    {}

    std::string method() { return _method_name; }

    bool paramCheck(const Json::Value &params) {
        for (auto &[name, vtype] : _param_desc) {
            if (params.isMember(name) == false) {
                ELOG("参数完整性校验失败，%s 字段缺失", name.c_str());
                return false;
            }
            if (check(params[name], vtype) == false) {
                ELOG("%s 参数的类型错误", name.c_str());
                return false;
            }
        }
        return true;
    }

    bool call(const Json::Value &params, Json::Value &result) {
        _callback(params, result);
        if (rtypeCheck(result) == false) {
            ELOG("回调函数信息校验失败！");
            return false;
        }
        return true;
    }

private:
    bool check(const Json::Value &value, VType type) {
        switch (type) {
            case VType::BOOL:
                return value.isBool();
            case VType::INTEGRA:
                return value.isIntegral();
            case VType::NUMERIC:
                return value.isNumeric();
            case VType::STRING:
                return value.isString();
            case VType::ARRAY:
                return value.isArray();
            case VType::OBJECT:
                return value.isObject();
            default:
                ELOG("未知类型");
                return false;
        }
    }

    bool rtypeCheck(const Json::Value &value) {
        return check(value, _return_type);
    }

private:
    std::string _method_name;                   // 方法名称
    ServiceCallback _callback;                  // 回调函数
    std::vector<ParamDescription> _param_desc;  // 参数描述字段
    VType _return_type;                         // 返回值类型描述
};

class SDFactory {
public:
    void setMethodName(const std::string name) { _method_name = name; }
    void setCallback(ServiceDescription::ServiceCallback cb) { _callback = cb; }
    void setParamDesc(const std::string pname, VType vtype) {
        _param_desc.push_back(std::make_pair(pname, vtype));
    }
    void setReturnType(VType rtype) { _return_type = rtype; }

    ServiceDescription::ptr build() {
        return std::make_shared<ServiceDescription>(
            std::move(_method_name), std::move(_callback),
            std::move(_param_desc), _return_type);
    }

private:
    std::string _method_name;                       // 方法名称
    ServiceDescription::ServiceCallback _callback;  // 回调函数
    std::vector<ServiceDescription::ParamDescription>
        _param_desc;     // 参数描述字段
    VType _return_type;  // 返回值类型描述
};

class ServiceManager {
public:
    using ptr = std::shared_ptr<ServiceManager>;
    void insert(const ServiceDescription::ptr &desc) {
        std::lock_guard<std::mutex> guard(_mutex);
        _services.insert(std::make_pair(desc->method(), desc));
    }
    ServiceDescription::ptr select(const std::string method_name) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _services.find(method_name);
        if (it == _services.end()) {
            return ServiceDescription::ptr();
        }
        return it->second;
    }
    void remove(const std::string method_name) {
        std::lock_guard<std::mutex> guard(_mutex);
        _services.erase(method_name);
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, ServiceDescription::ptr> _services;
};

class RpcRouter {
public:
    using ptr = std::shared_ptr<RpcRouter>;
    // 注册给dispatcher模块的回调函数
    void onRpcRequest(const wylrpc::BaseConnection::ptr &conn,
                      wylrpc::RpcRequest::ptr &req) {
        // 1.查看能否提供服务
        auto service = _manager->select(req->method());
        if (service.get() == nullptr) {
            ELOG("服务器没有 %s 服务", req->method().c_str());
            response(conn, req, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
            return;
        }
        // 2.检查参数是否正确
        bool ret = service->paramCheck(req->params());
        if (ret == false) {
            ELOG("%s 服务请求的参数错误", req->method().c_str());
            response(conn, req, Json::Value(), RCode::RCODE_INVALID_PARAMS);
            return;
        }
        // 3.调用回调函数
        Json::Value result;
        ret = service->call(req->params(), result);
        if (ret == false) {
            ELOG("回调函数处理过程中发生错误");
            response(conn, req, Json::Value(), RCode::RCODE_INTENAL_ERROR);
            return;
        }
        // 4.返回结果
        response(conn, req, result, RCode::RCODE_OK);
    }
    void registerHandler(const ServiceDescription::ptr &desc) {
        _manager->insert(desc);
    }

private:
    void response(const wylrpc::BaseConnection::ptr &conn,
                  wylrpc::RpcRequest::ptr &req, const Json::Value &res,
                  RCode rcode) {
        auto msg = MessageFactory::create<RpcResponse>();
        msg->setMType(MType::RSP_RPC);
        msg->setId(req->rid());
        msg->setRCode(rcode);
        msg->setResult(res);
        conn->send(msg);
    }

private:
    ServiceManager::ptr _manager;
};
}  // namespace server
}  // namespace wylrpc
