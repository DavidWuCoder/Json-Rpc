#include <thread>

#include "../../client/rpc_client.hpp"
void callback(const std::string &topic_name, const std::string &msg) {
    ILOG("%s 主题收到消息: %s", topic_name.c_str(), msg.c_str());
}
int main() {
    auto client =
        std::make_shared<wylrpc::client::TopicClient>("127.0.0.1", 8888);
    client->create("hello");
    client->subscribe("hello", callback);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    client->shutdown();
    return 0;
}
