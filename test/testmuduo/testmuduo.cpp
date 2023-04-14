#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include<string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

class ChatServer
{
public:
    ChatServer(EventLoop *loop,               // 事件循环
               const InetAddress &listenAddr, // IP + Port
               const string &nameArg)
        : _server(loop, listenAddr, nameArg), _loop(loop)
    {

        // 给服务器注册用户连接的创建和断开的回调
        _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));
        // 给服务器注册用户读写事件的回调
        _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));

        //设置服务器端的线程数量
        _server.setThreadNum(4);
    }
    void start(){
        _server.start();
    }

private:
    // 专门处理用户的连接和断开
    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected()){
            cout<< conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << "startOnline" << endl;
        }
        else{
            cout<< conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << "offOnline" << endl;
            conn->shutdown();
        }
    }
    // 专门处理用户的读写事件
    void onMessage(const TcpConnectionPtr &conn,//连接
                   Buffer *buffer,    //缓冲区
                   Timestamp time)   //接收到数据的时间信息
    {
        string buf = buffer->retrieveAllAsString();
        cout << "revr data:" << buf << " time" << time.toString() << endl;
        conn->send(buf);
    }
    TcpServer _server; // #1
    EventLoop *_loop;  // #2
};

int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6001);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    return 0;
}