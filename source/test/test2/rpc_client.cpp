#include "../../client/rpc_client.hpp"

#include <thread>

#include "../../common/detail.hpp"
void callback(const Json::Value& result) { ILOG("result: %d", result.asInt()); }

int main() {
    std::cout << "111111" << std::endl;
    wylrpc::client::RpcClient client(true, "127.0.0.1", 9091);

    Json::Value params;
    params["num1"] = 11;
    params["num2"] = 22;
    Json::Value result;
    bool ret = client.call("Add", params, result);
    if (ret != false) {
        DLOG("-----------------------------");
        std::cout << "result: " << result.asInt() << std::endl;
    }

    params["num1"] = 33;
    params["num2"] = 44;
    wylrpc::client::RpcCaller::JsonAsyncReponse future_res;
    ret = client.call("Add", params, future_res);
    if (ret != false) {
        DLOG("-----------------------------");
        result = future_res.get();
        std::cout << "result: " << result.asInt() << std::endl;
    }

    params["num1"] = 55;
    params["num2"] = 66;
    ret = client.call("Add", params, callback);

    DLOG("-----------------------------");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
