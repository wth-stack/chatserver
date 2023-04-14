#include "groupmodel.hpp"
#include "db.h"

//创建群组
bool GroupModel::createGroup(Group &group){
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values(%s, '%s')", group.getName().c_str(), group.getDesc().c_str());
    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            group.setId(mysql_insert_id(mysql.getconnect()));
            return true;
        }
    }
    return false;
}
//加入群组
void GroupModel::addGroup(int userid, int groupid, string role){
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d, '%d', '%s')", groupid, userid, role.c_str());
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}
//查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid){
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.groupname, a.groupdesc from allgroup a inner join \
                    groupuser b on a.id = b.groupid where b.userid = %d", userid);
    MySQL mysql;
    vector<Group> result;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(res != NULL){
            MYSQL_ROW row;
            while((row =  mysql_fetch_row(res)) != NULL){
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                result.push_back(group);
            }

            mysql_free_result(res);
        }
    }

    //查询群组的用户信息
    for(Group &group : result){
        sprintf(sql, "select a.id, a.name, a.state, b.grouprole from user a inner join \
                    groupuser b on b.userid = a.id where b.groupid = %d", group.getId());
        MYSQL_RES *res = mysql.query(sql);
        if(res != NULL){
            MYSQL_ROW row;
            while((row =  mysql_fetch_row(res)) != NULL){
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUser().push_back(user);
            }

            mysql_free_result(res);
        }
    }
    return result;
}
//根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其他成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid){
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", groupid, userid);
    MySQL mysql;
    vector<int> idresult;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(res != NULL){
            MYSQL_ROW row;
            while((row =  mysql_fetch_row(res)) != NULL){
                idresult.push_back(atoi(row[0]));
            }

            mysql_free_result(res);
        }
    }
    return idresult;
}