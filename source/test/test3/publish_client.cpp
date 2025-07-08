#include "../../client/rpc_client.hpp"

int main() {
    auto client =
        std::make_shared<wylrpc::client::TopicClient>("127.0.0.1", 8888);
    bool ret = client->create("hello");
    if (!ret) {
        ELOG("主题创建失败");
    }
    for (int i = 0; i < 10; i++) {
        client->publish("hello", "Hello Wrold!-" + std::to_string(i));
    }
    client->shutdown();
    return 0;
}
