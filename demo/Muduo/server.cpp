/*
    实现一个翻译服务器，客服端发送英文单词，服务端返回中文
*/

#include <muduo/net/TcpServer.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <string>
#include <unordered_map>

class DictServer
{
    public:
        DictServer(int port)
            : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "DictServer", muduo::net::TcpServer::kReusePort)
        {
            // 设置两种回调函数
            _server.setConnectionCallback(std::bind(&DictServer::onConnection, this, std::placeholders::_1));
            _server.setMessageCallback(std::bind(&DictServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        }

        void start()
        {
            // 启动服务器
            _server.start();
            // 开始事件监听循环
            _baseloop.loop();
        }

    private:
        // 连接和关闭时的回调函数
        void onConnection(const muduo::net::TcpConnectionPtr &coon)
        {
            if (coon->connected())
            {
                std::cout << "连接建立！" << std::endl;
            }
            else
            {
                std::cout << "连接失败！" << std::endl;
            }
        }   
        // 收到消息时的回调函数
        void onMessage(const muduo::net::TcpConnectionPtr &coon, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            static std::unordered_map<std::string, std::string> dict_map = {{"hello", "你好"}, {"world", "世界"}};
            std::string msg = buf->retrieveAllAsString();
            std::string res;
            auto it = dict_map.find(msg);
            if (it != dict_map.end())
            {
                res = it->second;
            }
            else
            {
                res = "未知单词!";
            }
            coon->send(res);
        }
        muduo::net::EventLoop _baseloop;
        muduo::net::TcpServer _server;
};

int main()
{
    DictServer server(8080);
    server.start();
    return 0;
}