#include <jsoncpp/json/value.h>
#include <jsoncpp/json/version.h>

#include <future>
#include <memory>

#include "requestor.hpp"

namespace wylrpc {
namespace client {
class RpcCaller {
public:
    using ptr = std::shared_ptr<RpcCaller>;
    using JsonAsyncReponse = std::future<Json::Value>;
    using JsonResponseCallback = std::function<void(const Json::Value &)>;
    RpcCaller(const Requestor::ptr &requestor) : _requestor(requestor) {}

    bool call(const BaseConnection::ptr &conn, const std::string &method,
              const Json::Value &params, Json::Value &result) {
        DLOG("开始同步rpc调用");
        // 1.组织请求
        auto req_msg = MessageFactory::create<RpcRequest>();
        req_msg->setId(UUID::uuid());
        req_msg->setMType(MType::REQ_RPC);
        req_msg->setMethod(method);
        req_msg->setParams(params);
        BaseMessage::ptr rsp_msg;
        // 2.发送请求
        bool ret = _requestor->send(
            conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), rsp_msg);
        if (ret == false) {
            ELOG("同步请求发送失败");
            return false;
        }
        // 3.等待响应
        DLOG("收到响应, 等待处理");
        auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
        if (!rpc_rsp_msg) {
            ELOG("响应类型转换失败");
            return false;
        }
        if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) {
            ELOG("Rpc处理出错，%s ", errReason(rpc_rsp_msg->rcode()).c_str());
            return false;
        }
        result = rpc_rsp_msg->result();
        DLOG("结果设置完毕");
        return true;
    }
    bool call(const BaseConnection::ptr &conn, const std::string &method,
              const Json::Value &params, JsonAsyncReponse &result) {
        // 调用异步回调函数，回调函数中传入一个promise,对promise设置结果
        // 1.组织请求
        auto req_msg = MessageFactory::create<RpcRequest>();
        req_msg->setId(UUID::uuid());
        req_msg->setMType(MType::REQ_RPC);
        req_msg->setMethod(method);
        req_msg->setParams(params);

        auto json_promise = std::make_shared<std::promise<Json::Value>>();
        Requestor::RequestCallback cb =
            std::bind(&RpcCaller::CallbackAsync, this, json_promise,
                      std::placeholders::_1);
        // 2.发送请求
        bool ret = _requestor->send(
            conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), cb);
        if (ret == false) {
            ELOG("异步请求发送失败");
            return false;
        }
        return true;
    }
    bool call(const BaseConnection::ptr &conn, const std::string &method,
              const Json::Value &params, JsonResponseCallback &cb) {
        // 1.组织请求
        auto req_msg = MessageFactory::create<RpcRequest>();
        req_msg->setId(UUID::uuid());
        req_msg->setMType(MType::REQ_RPC);
        req_msg->setMethod(method);
        req_msg->setParams(params);
        Requestor::RequestCallback req_cb =
            std::bind(&RpcCaller::Callback, this, cb, std::placeholders::_1);
        // 2.发送请求
        bool ret = _requestor->send(
            conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), req_cb);
        if (ret == false) {
            ELOG("Rpc回调请求发送失败");
            return false;
        }
        return true;
    }

private:
    void Callback(JsonResponseCallback &cb, const BaseMessage::ptr &msg) {
        DLOG("收到响应, 等待处理");
        auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
        if (!rpc_rsp_msg) {
            ELOG("响应类型转换失败");
            return;
        }
        if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) {
            ELOG("Rpc异步出错，%s ", errReason(rpc_rsp_msg->rcode()).c_str());
            return;
        }
        cb(rpc_rsp_msg->result());
    }
    void CallbackAsync(std::shared_ptr<std::promise<Json::Value>> result,
                       const BaseMessage::ptr &msg) {
        DLOG("收到响应, 等待处理");
        auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
        if (!rpc_rsp_msg) {
            ELOG("响应类型转换失败");
            return;
        }
        if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) {
            ELOG("Rpc异步出错，%s ", errReason(rpc_rsp_msg->rcode()).c_str());
            return;
        }
        result->set_value(rpc_rsp_msg->result());
    }

private:
    Requestor::ptr _requestor;
};
}  // namespace client
}  // namespace wylrpc
