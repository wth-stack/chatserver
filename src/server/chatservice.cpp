#include "public.hpp"
#include "chatservice.hpp"
#include "muduo/base/Logging.h"
#include "offlinemessagemodel.hpp"
using namespace std;
using namespace muduo::net;
// 获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
};

ChatService::ChatService(){
    _msgHandlerMap.insert({LOGIN_MAS, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MAS, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    //连接redis服务器
    if(_redis.connect()){
        //设置上报信息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time){
    LOG_INFO << "do login server";
    int id = js["id"].get<int>();
    string password = js["password"];

    User user = _userModel.query(id);
    if(user.getId() == id && user.getPassword() == password){
        if(user.getState() == "online"){
            //该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MAS_ACK;
            response["error"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else{
            {
                //登录成功记录用户连接信息
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            //id用户登录成功后，向Redis订阅channel(id)
            _redis.subscribe(id);

            //登陆成功，更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MAS_ACK;
            response["error"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            vector<string> vec = _offlinemsg.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                //读取该用户的离线消息,并删除
                _offlinemsg.remove(id);
            }
            //查询该用户的好友消息，并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(User &user : userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2; 
            }
            //查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty()){
                //group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for(Group &group : groupuserVec){
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();

                    vector<string> userV;
                    for(GroupUser &user : group.getUser()){
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            //查询用户是否有离线消息
            conn->send(response.dump());

        }

    }
    else{
        //登陆失败
        json response;
        response["msgid"] = LOGIN_MAS_ACK;
        response["error"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
};

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        } 
    }
    //用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

//服务器异常，业务重置方法
void ChatService::reset(){
    //把online状态的用户，设置成offline
    _userModel.resetstate();
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time){
    LOG_INFO << "do reg server";
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    bool state = _userModel.insert(user);
    //注册成功
    if(state){
        json response;
        response["msgid"] = REG_MAS_ACK;
        response["error"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    //注册失败
    else{
        json response;
        response["msgid"] = REG_MAS_ACK;
        response["error"] = 1;
        conn->send(response.dump());
    }
};

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int toid = js["to"].get<int>();
    bool userState = false;
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            //toid在线，转发消息  服务器主动推送消息给toid用户
            it->second->send(js.dump());

            return;
        }
    }
    // 查询toid是否在线
    User user = _userModel.query(toid);
    if(user .getState() == "online"){
        _redis.publish(toid, js.dump());
        return;
    }

    //toid不在线，存储离线消息
    _offlinemsg.insert(toid, js.dump());
}

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);

}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid){
    //记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){
        //返回一个默认的处理器
        return [=](const TcpConnectionPtr &conn, json &json, Timestamp){
            LOG_ERROR << "msgid" << msgid << "can not find handler!";
        };
    }
    else{
        return _msgHandlerMap[msgid];
    }
};


//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); it++){
            if(it->second == conn){
                user.setId(it->first);
                // 从map表删除用户的链接信息
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    if(user.getId() != -1){
        //更新用户的状态信息
        user.setState("offline");
        _userModel.updateState(user);
    }
}
//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1, name,desc);
    if(_groupModel.createGroup(group)){
        //存储群组创建人的信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}
//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid,"normal");
}
//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec){
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()){
            //转发群消息
            it->second->send(js.dump());
        }
        else{
            // 查询toid是否在线
            User user = _userModel.query(id);
            if(user .getState() == "online"){
                _redis.publish(id, js.dump());
                return;
            }
            else{
                //存储离线群消息
                _offlinemsg.insert(id, js.dump());
            }

        }
    }
}

//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg){
    json js = json::parse(msg.c_str());

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end()){
        it->second->send(msg);
        return;
    }
    //存储该用户的离线消息
    _offlinemsg.insert(userid, msg);
}