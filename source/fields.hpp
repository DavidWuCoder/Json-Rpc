#pragma once
#include <string>

namespace wylrpc {
#define KEY_METHOD "methods"
#define KEY_PARAMS "parameters"
#define KEY_TOPIC_KEY "tpoic_key"
#define KEY_TOPIC_MSG "tpoic_msg"
#define KEY_OPTYPE "optype"
#define KEY_HOST "host"
#define KEY_HOST_IP "ip"
#define KEY_HOST_PORT "port"
#define KEY_RCODE "rcode"
#define KEY_RESULT "result"

// 消息类型
enum class MType {
    REQ_RPC = 0,
    RSP_RPC,
    REQ_TOPIC,
    RSP_TOPIC,
    REQ_SERVICE,
    RSP_SERVICE
};

// 定义响应码
enum class RCode {
    RCODE_OK = 0,
    RCODE_PARSE_FAILED,
    RCODE_ERROR_MSGTYPE,
    RCODE_INVALID_MSG,
    RCODE_DISCONNECTED,
    RCODE_INVALID_PARAMS,
    RCODE_NOT_FOUND_SERVICE,
    RCODE_INVALID_OPTYPE,
    RCODE_NOT_FOUND_TOPIC,
    RCODE_INTERNAL_ERROR,
};

// 通过响应码查询响应类型的接口
static std::string errReason(RCode code) {
    std::string err_res;
    switch (code) {
        case RCode::RCODE_OK:
            err_res = "成功!";
            break;
        case RCode::RCODE_PARSE_FAILED:
            err_res = "消息解析失败!";
            break;
        case RCode::RCODE_ERROR_MSGTYPE:
            err_res = "消息类型错误!";
            break;
        case RCode::RCODE_INVALID_MSG:
            err_res = "无效消息!";
            break;
        case RCode::RCODE_DISCONNECTED:
            err_res = "连接已断开!";
            break;
        case RCode::RCODE_INVALID_PARAMS:
            err_res = "无效的RPC参数!";
            break;
        case RCode::RCODE_NOT_FOUND_SERVICE:
            err_res = "找不到对应的服务!";
            break;
        case RCode::RCODE_INVALID_OPTYPE:
            err_res = "无效的操作类型!";
            break;
        case RCode::RCODE_NOT_FOUND_TOPIC:
            err_res = "找不到对应主题!";
            break;
        case RCode::RCODE_INTERNAL_ERROR:
            err_res = "内部错误!";
            break;
        default:
            err_res = "未知错误！";
            break;
    }
    return err_res;
}
}  // namespace wylrpc
