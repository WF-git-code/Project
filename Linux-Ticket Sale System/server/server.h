// 预约系统服务器 - epoll + 线程池 + MySQL连接池
//
// 这个文件是服务器的头文件，包含所有类的声明和常量定义
// 主要功能：
// 1. 配置管理
// 2. Socket多态设计
// 3. MySQL客户端
// 4. TCP服务器

#ifndef SERVER_H
#define SERVER_H

// 标准头文件
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

// JSON处理库
#include <jsoncpp/json/json.h>

// MySQL库
#include <mysql/mysql.h>

// 项目自定义头文件
#include "ThreadPool/CachedThreadPool.hpp"
#include "LogFile/include/LogCommon.hpp"
#include "LogFile/include/LogMessage.hpp"
#include "LogFile/include/Logger.hpp"
#include "LogFile/include/LogFile.hpp"
#include "LogFile/include/AsynLogging.hpp"
#include "LogFile/include/LoggerManager.hpp"
#include "MysqlConnectionPool.hpp"
#include "RedisConnectionPool.hpp"
#include "Protocol.hpp"

using namespace std;

// 操作类型枚举，用于区分不同的客户端请求
// 这是一个枚举，每个值代表一种用户操作
enum OpType {
    LOGIN = 1,          // 用户登录操作
    REGISTER = 2,       // 用户注册操作
    SHOW_TICKET = 3,    // 查看所有可预约的票务
    BOOK_TICKET = 4,    // 预约票务
    MY_BOOKINGS = 5,    // 查看我的预约记录
    CANCEL_BOOKING = 6, // 取消预约
    EXIT = 7            // 用户退出
};

// epoll最大监听事件数
// epoll_wait每次最多处理这么多事件
const int EPOLL_MAX_EVENTS = 128;

// 服务器配置类
// 这个类用来存储和读取服务器的配置文件
// 配置包括：监听地址、线程池大小、数据库连接、Redis连接等
class ServerConfig
{
public:
    // 构造函数，设置默认配置值
    ServerConfig() {
        // 网络配置
        listen_ip = "127.0.0.1";    // 默认监听本地回环地址
        listen_port = 6000;         // 默认监听6000端口
        listen_max = 1024;          // 最大监听队列长度
        
        // 线程池配置
        thread_core_num = 4;        // 核心线程数（常驻线程）
        thread_max_num = 17;        // 最大线程数
        task_queue_max = 1024;      // 任务队列最大长度
        task_wait_time_us = 10;     // 任务等待时间（微秒）
        thread_wait_time_s = 10;    // 线程空闲等待时间（秒）
        
        // 日志配置
        enable_log_clean = 1;       // 是否启用日志清理
        log_keep_days = 7;          // 日志保留天数
        log_dir = "./";             // 日志文件目录
        
        // MySQL数据库配置
        db_host = "127.0.0.1";      // 数据库地址
        db_port = 3306;             // 数据库端口
        db_name = "epoll_project";  // 数据库名称
        db_user = "root";           // 数据库用户名
        db_password = "211929";     // 数据库密码
        db_pool_size = 8;           // 数据库连接池大小
        
        // Redis缓存配置
        redis_host = "127.0.0.1";   // Redis地址
        redis_port = 6379;          // Redis端口
        redis_password = "";        // Redis密码（空表示无密码）
        redis_pool_size = 4;        // Redis连接池大小
        redis_ttl = 300;            // 缓存过期时间（秒）
    }

    // 从文件读取配置
    bool ReadConf(string filename);
    
    // 打印当前配置信息（用于调试）
    void PrintInfo();

public:
    // 网络配置成员变量
    string listen_ip;      // 监听IP地址
    short listen_port;     // 监听端口
    int listen_max;        // 最大监听队列长度
    
    // 线程池配置
    int thread_core_num;    // 核心线程数
    int thread_max_num;     // 最大线程数
    int task_queue_max;     // 任务队列最大长度
    int task_wait_time_us;  // 任务等待时间
    int thread_wait_time_s; // 线程空闲等待时间
    
    // 日志配置
    int enable_log_clean;  // 是否启用日志清理
    int log_keep_days;     // 日志保留天数
    string log_dir;        // 日志目录
    
    // MySQL数据库配置
    string db_host;        // 数据库主机地址
    short db_port;         // 数据库端口
    string db_name;        // 数据库名称
    string db_user;        // 数据库用户名
    string db_password;    // 数据库密码
    int db_pool_size;      // 连接池大小

    // Redis缓存配置
    string redis_host;     // Redis主机地址
    int redis_port;        // Redis端口
    string redis_password; // Redis密码
    int redis_pool_size;   // Redis连接池大小
    int redis_ttl;         // 缓存过期时间
};

// Socket基类的前向声明
// 这个Socket是我们自己定义的类，不是系统的socket函数
class Socket;

// MySQL数据库客户端类
// 这个类封装了所有与数据库交互的操作
// 不再持有单个连接，而是使用连接池获取连接
class MysqlClient
{
public:
    MysqlClient() = default;      // 默认构造函数
    ~MysqlClient() = default;     // 默认析构函数

    // 用户注册操作：插入新用户到数据库
    bool Db_User_Register(const string &tel, const string &name, const string &passwd);
    
    // 用户登录操作：验证用户名和密码
    bool Db_User_Login(const string &tel, string &name, const string &passwd);
    
    // 查看所有可预约的票务：从数据库查询并返回
    bool Db_Show_Ticket(Json::Value &res);
    
    // 预约票务：扣减库存并插入预约记录（事务+分布式锁）
    bool Db_Yd_Ticket(string& usertel, string& ticketid);
    
    // 查看我的预约记录：查询用户的预约历史
    bool Db_Get_Yuyue(string& usertel, Json::Value& res);
    
    // 取消预约：恢复库存并删除预约记录（事务+分布式锁）
    bool Db_Cancel_Yuyue(string& usertel, string& ticketid);
};

// Socket基类 - 使用多态设计模式
// 这个设计很巧妙！我们有两种Socket：
// 1. ListenSocket：用来监听新连接
// 2. ConnectSocket：用来和客户端通信
// 它们都继承自Socket基类，通过虚函数Handle_Data()实现多态
class Socket
{
public:
    // 构造函数：保存文件描述符和epoll句柄
    Socket(int fd, int epfd) : m_fd(fd), m_epfd(epfd) {}
    
    // 析构函数：关闭socket文件描述符
    virtual ~Socket() { close(m_fd); }
    
    // 纯虚函数：处理数据（子类必须实现）
    virtual void Handle_Data() = 0;

    int m_fd;       // socket文件描述符
    int m_epfd;     // epoll文件描述符（用于注册事件）
};

// 监听Socket类 - 专门用来accept新的客户端连接
// 当有新客户端连接时，这个类的Handle_Data()会被调用
class ListenSocket : public Socket
{
public:
    // 构造函数：调用基类构造函数
    ListenSocket(int fd, int epfd) : Socket(fd, epfd) {}
    
    // 处理新连接：accept新连接并创建ConnectSocket
    void Handle_Data();

private:
    // 重新注册epoll事件（因为EPOLLONESHOT会一次性触发）
    void ResetEvent();
    
    // 静态变量：记录当前连接数（用于调试）
    static int connection_count;
};

// 连接Socket类 - 专门用来和已连接的客户端通信
// 当客户端发送数据时，这个类的Handle_Data()会被调用
class ConnectSocket : public Socket
{
public:
    // 构造函数：调用基类构造函数，初始化操作类型
    ConnectSocket(int fd, int epfd) : Socket(fd, epfd) {
        m_op_type = -1;
    }

    // 处理客户端数据：解析请求，处理业务，返回响应
    void Handle_Data();

private:
    int m_op_type;            // 解析出的操作类型
    Json::Value m_request;    // 解析出的JSON请求数据
    ProtocolHandler m_protocol; // 自定义TCP协议处理器，解决粘包拆包

    // 重新注册epoll事件（EPOLLONESHOT）
    void ResetEvent();
    
    // 从请求数据中解析出操作类型
    void Get_OpType(char buff[]);
    
    // 发送OK响应给客户端
    void Send_OK();
    
    // 发送ERR响应给客户端
    void Send_ERR();
    
    // 发送JSON对象给客户端
    void Send_Jsonobj(Json::Value &root);
    
    // 用户注册处理函数
    void User_Register();
    
    // 用户登录处理函数
    void User_Login();
    
    // 查看票务处理函数
    void Show_Ticket();
    
    // 预约票务处理函数
    void Yd_Ticket();
    
    // 查看我的预约处理函数
    void Show_My_Yuyue();
    
    // 取消预约处理函数
    void Cancel_Yuyue();
};

// TCP服务器主类
// 这是整个服务器的核心，负责初始化和运行
class TcpServer
{
public:
    // 构造函数：初始化配置和线程池
    TcpServer(ServerConfig &sc) : m_config(sc), 
        m_thread_pool(sc.thread_core_num, sc.task_queue_max, 
            sc.task_wait_time_us, sc.thread_wait_time_s, sc.thread_max_num) {
        m_listen_fd = -1;  // 初始化监听文件描述符为-1
        m_epoll_fd = -1;   // 初始化epoll文件描述符为-1
    }

    // 服务器初始化：创建socket，创建epoll
    bool Ser_Init();
    
    // 服务器主循环：epoll_wait等待事件，处理事件
    void Run();

private:
    // 创建监听socket：socket -> bind -> listen
    bool create_socket();
    
    // 处理就绪事件：遍历所有事件，提交到线程池处理
    void do_events();

private:
    ServerConfig m_config;                     // 服务器配置
    int m_listen_fd;                           // 监听socket的文件描述符
    int m_epoll_fd;                            // epoll的文件描述符
    struct epoll_event m_events[EPOLL_MAX_EVENTS]; // epoll事件数组
    int m_event_count;                         // 本次epoll_wait返回的事件数
    CachedThreadPool m_thread_pool;            // 线程池（处理业务逻辑）
};

#endif // SERVER_H
