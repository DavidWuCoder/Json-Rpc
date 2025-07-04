#include <pthread.h>

#include <future>
#include <memory>
#include <mutex>

#include "../common/message.hpp"
#include "../common/net.hpp"

namespace wylrpc {
namespace client {
class Requestor {
public:
    using RequestCallback = std::function<void(BaseMessage::ptr)>;
    using AsyncResponse = std::future<BaseMessage::ptr>;
    using ptr = std::shared_ptr<Requestor>;
    struct RequestDescription {
        using ptr = std::shared_ptr<RequestDescription>;
        BaseMessage::ptr request;
        RType rtype;
        std::promise<BaseMessage::ptr> response;
        RequestCallback callback;
    };
    void onResponse(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) {
        std::string rid = msg->rid();
        RequestDescription::ptr rdp = getDescription(rid);
        if (rdp.get() == nullptr) {
            ELOG("收到响应 %s ,未找到对应的请求描述", rid.c_str());
            return;
        }
        if (rdp->rtype == RType::REQ_ASYNC) {
            rdp->response.set_value(msg);
        } else if (rdp->rtype == RType::REQ_CALLBACK) {
            rdp->callback(msg);
        } else {
            ELOG("未知请求类型");
        }
        // 处理完立即删除，防止内存积压
        delDescription(rid);
    }
    bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req,
              AsyncResponse &async_rsp) {
        RequestDescription::ptr rdp = newDescription(req, RType::REQ_ASYNC);
        if (rdp.get() == nullptr) {
            ELOG("构造请求对象失败了");
            return false;
        }
        conn->send(req);
        async_rsp = rdp->response.get_future();
        return true;
    }
    bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req,
              BaseMessage::ptr &rsp) {
        AsyncResponse rsp_future;
        bool ret = send(conn, req, rsp_future);
        if (ret == false) {
            return false;
        }
        rsp = rsp_future.get();
        return true;
    }
    bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req,
              RequestCallback cb) {
        RequestDescription::ptr rdp =
            newDescription(req, RType::REQ_CALLBACK, cb);
        if (rdp.get() == nullptr) {
            ELOG("构造请求对象失败了");
            return false;
        }
        conn->send(req);
        return true;
    }

private:
    RequestDescription::ptr newDescription(
        const BaseMessage::ptr &req, RType type,
        const RequestCallback &cb = RequestCallback()) {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto rd = std::make_shared<RequestDescription>();
            rd->request = req;
            rd->rtype = type;
            if (type == RType::REQ_CALLBACK && cb) {
                rd->callback = cb;
            }
            _request_desc.insert(std::make_pair(req->rid(), rd));
            return rd;
        }
    }
    RequestDescription::ptr getDescription(const std::string &rid) {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _request_desc.find(rid);
            if (it == _request_desc.end()) {
                return RequestDescription::ptr();
            }
            return it->second;
        }
    }
    void delDescription(const std::string &rid) {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            _request_desc.erase(rid);
        }
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, RequestDescription::ptr> _request_desc;
};
}  // namespace client
}  // namespace wylrpc
