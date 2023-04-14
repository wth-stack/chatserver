#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <map>
#include <functional>
#include<mutex>
#include <muduo/net/TcpConnection.h>
using namespace muduo::net;
using namespace muduo;
using namespace std;

#include "redis.hpp"
#include "model/usermodel.hpp"
#include "model/friendmodel.hpp"
#include "model/groupmodel.hpp"
#include "model/offlinemessagemodel.hpp"
#include "json.hpp"
using json = nlohmann::json;
//表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &json, Timestamp)>;

//聊天服务器业务类
class ChatService
{
private:
    ChatService();
    //存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    //数据操作类
    UserModel _userModel;
    FriendModel _friendModel;
    GroupModel _groupModel;


    //定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;

    //存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    //存储连线用户信息
    OfflineMsgModel _offlinemsg;

    //redis操作对象
    Redis _redis;
public:
    // 获取单例对象的接口函数
    static ChatService* instance();
    //处理登录业务
    void login(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &json, Timestamp time);
    //从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int userid, string msg);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    //服务器异常，业务重置方法
    void reset();
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
};

#endif