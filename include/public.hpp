#ifndef PUBLIC_H
#define PUBLIC_H

/*
server和client的公共文件
**/
enum EnMsgType{
    LOGIN_MAS = 1, //登录消息
    LOGIN_MAS_ACK, //登录相应消息
    REG_MAS,   //注册消息
    REG_MAS_ACK, //注册相应消息
    ONE_CHAT_MSG, //聊天消息
    ADD_FRIEND_MSG, //添加好友消息

    CREATE_GROUP_MSG, //创建群组
    ADD_GROUP_MSG,     //加入群组
    GROUP_CHAT_MSG,  //群聊天
    LOGINOUT_MSG, //注销消息

};
#endif