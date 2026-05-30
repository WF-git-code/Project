// 预约系统服务器 - epoll + 线程池 + MySQL连接池

#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <sys/epoll.h>
#include <memory>
#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>
#include "ThreadPool/CachedThreadPool.hpp"
#include "LogFile/include/LogCommon.hpp"
#include "LogFile/include/LogMessage.hpp"
#include "LogFile/include/Logger.hpp"
#include "LogFile/include/LogFile.hpp"
#include "LogFile/include/AsynLogging.hpp"
#include "LogFile/include/LoggerManager.hpp"
#include "MysqlConnectionPool.hpp"
#include "RedisConnectionPool.hpp"

using namespace std;

// 操作类型枚举
enum OpType {
    LOGIN = 1,
    REGISTER = 2,
    SHOW_TICKET = 3,
    BOOK_TICKET = 4,
    MY_BOOKINGS = 5,
    CANCEL_BOOKING = 6,
    EXIT = 7
};

// epoll最大监听事件数
const int EPOLL_MAX_EVENTS = 128;

// 服务器配置类
class ServerConfig
{
public:
    ServerConfig() {
        listen_ip = "127.0.0.1";
        listen_port = 6000;
        listen_max = 1024;
        thread_core_num = 4;
        thread_max_num = 17;
        task_queue_max = 1024;
        task_wait_time_us = 10;
        thread_wait_time_s = 10;
        enable_log_clean = 1;
        log_keep_days = 7;
        log_dir = "./";
        // 数据库配置
        db_host = "127.0.0.1";
        db_port = 3306;
        db_name = "epoll_project";
        db_user = "root";
        db_password = "211929";
        db_pool_size = 8;
        // Redis配置
        redis_host = "127.0.0.1";
        redis_port = 6379;
        redis_password = "";
        redis_pool_size = 4;
        redis_ttl = 300;
    }

    bool ReadConf(string filename);
    void PrintInfo();

public:
    string listen_ip;
    short listen_port;
    int listen_max;
    int thread_core_num;
    int thread_max_num;
    int task_queue_max;
    int task_wait_time_us;
    int thread_wait_time_s;
    
    // 日志清理配置
    int enable_log_clean;
    int log_keep_days;
    string log_dir;
    
    // 数据库配置
    string db_host;
    short db_port;
    string db_name;
    string db_user;
    string db_password;
    int db_pool_size;

    // Redis配置
    string redis_host;
    int redis_port;
    string redis_password;
    int redis_pool_size;
    int redis_ttl;
};

// 前向声明
class Socket;

// MySQL数据库客户端类
// 现在使用连接池获取连接，不再持有单个连接
class MysqlClient
{
public:
    MysqlClient() = default;
    ~MysqlClient() = default;

    // 不再需要Connect_MysqlServer，连接由连接池管理
    bool Db_User_Register(const string &tel, const string &name, const string &passwd);
    bool Db_User_Login(const string &tel, string &name, const string &passwd);
    bool Db_Show_Ticket(Json::Value& res);
    bool Db_Yd_Ticket(string& usertel, string& ticketid);
    bool Db_Get_Yuyue(string& usertel, Json::Value& res);
    bool Db_Cancel_Yuyue(string& usertel, string& ticketid);
};

// Socket基类 - 用多态区分监听socket和连接socket
class Socket
{
public:
    Socket(int fd, int epfd) : m_fd(fd), m_epfd(epfd) {}
    virtual ~Socket() { close(m_fd); }
    virtual void Handle_Data() = 0;

    int m_fd;
    int m_epfd;
};

// 监听Socket - accept新连接
class ListenSocket : public Socket
{
public:
    ListenSocket(int fd, int epfd) : Socket(fd, epfd) {}
    void Handle_Data();

private:
    void ResetEvent();
    static int connection_count;
};

// 连接Socket - 处理客户端请求
class ConnectSocket : public Socket
{
public:
    ConnectSocket(int fd, int epfd) : Socket(fd, epfd) {
        m_op_type = -1;
    }

    void Handle_Data();

private:
    int m_op_type;
    Json::Value m_request;

    void ResetEvent();
    void Get_OpType(char buff[]);
    void Send_OK();
    void Send_ERR();
    void Send_Jsonobj(Json::Value &root);
    void User_Register();
    void User_Login();
    void Show_Ticket();
    void Yd_Ticket();
    void Show_My_Yuyue();
    void Cancel_Yuyue();
};

// TCP服务器主类
class TcpServer
{
public:
    TcpServer(ServerConfig &sc) : m_config(sc), 
        m_thread_pool(sc.thread_core_num, sc.task_queue_max, 
            sc.task_wait_time_us, sc.thread_wait_time_s, sc.thread_max_num) {
        m_listen_fd = -1;
        m_epoll_fd = -1;
    }

    bool Ser_Init();
    void Run();

private:
    bool create_socket();
    void do_events();

private:
    ServerConfig m_config;
    int m_listen_fd;
    int m_epoll_fd;
    struct epoll_event m_events[EPOLL_MAX_EVENTS];
    int m_event_count;
    CachedThreadPool m_thread_pool;
};

#endif
