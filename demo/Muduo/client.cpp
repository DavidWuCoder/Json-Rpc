#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/base/CountDownLatch.h>
#include <iostream>
#include <string>
#include <unordered_map>

class DictClient
{
    public:
        DictClient(const std::string& server_ip, int server_port)
            : _baseloop(_loopthread.startLoop()) // 客户端只能交给一个线程区进行事件循环
            , _downlatch(1)
            , _client(_baseloop, muduo::net::InetAddress(server_ip, server_port), "DictClient")
        {
            // 设置两种回调函数
            _client.setConnectionCallback(std::bind(&DictClient::onConnection, this, std::placeholders::_1));
            _client.setMessageCallback(std::bind(&DictClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

            // 连接服务器，等待连接完成
            _client.connect();
            _downlatch.wait();
        }

        bool send(const std::string msg)
        {
            if (!_conn->connected())
            {
                std::cout << "连接已经断开，发送数据失败！" << std::endl;
                return false;
            }
            _conn->send(msg);
            return true;
        }

    private:
        // 连接和关闭时的回调函数
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                std::cout << "连接建立！" << std::endl;
                _downlatch.countDown(); // downlatch计数--
                _conn = conn;
            }
            else
            {
                std::cout << "连接失败！" << std::endl;
                _conn.reset();
            }
        }
        // 收到消息时的回调函数
        void onMessage(const muduo::net::TcpConnectionPtr &coon, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            std::string res = buf->retrieveAllAsString();
            std::cout << res << std::endl;
        }

    private:
        muduo::net::TcpConnectionPtr _conn;
        muduo::CountDownLatch _downlatch;
        muduo::net::EventLoopThread _loopthread;
        muduo::net::EventLoop *_baseloop;
        muduo::net::TcpClient _client;
};

int main()
{
    DictClient client("127.0.0.1", 8080);
    while (1)
    {
        std::string msg;
        std::cin >> msg;
        client.send(msg);
    }
    return 0;
}