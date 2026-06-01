// 预约系统客户端
//
// 这个文件是客户端的头文件，包含TCP客户端类的声明
// 主要功能：
// 1. 连接服务器
// 2. 用户注册/登录
// 3. 查看/预约/取消票务
// 4. 与服务器通信（使用自定义协议）
// 5. 文件上传（支持大文件和断点续传）

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <jsoncpp/json/json.h>

// 引入服务器的协议头文件
#include "../server/Protocol.hpp"

using namespace std;

// 操作类型枚举（与服务器保持一致！）
enum OpType
{
    LOGIN = 1,          // 登录操作
    REGISTER = 2,       // 注册操作
    SHOW_TICKET = 3,    // 查看票务
    BOOK_TICKET = 4,    // 预约票务
    MY_BOOKINGS = 5,    // 我的预约
    CANCEL_BOOKING = 6, // 取消预约
    UPLOAD_FILE = 8,    // 文件上传
    EXIT = 9            // 退出
};

// TCP客户端类
class TcpClient
{
public:
    // 带参数的构造函数
    // 参数：服务器IP地址、服务器端口号
    TcpClient(string ser_ip, short ser_port)
    {
        ips = ser_ip;       // 保存服务器IP
        port = ser_port;    // 保存服务器端口
        runing = true;      // 标记运行状态为真
        login_status = false;  // 初始状态未登录
        op_type = -1;       // 操作类型初始化为-1
        sockfd = -1;
    }

    // 默认构造函数
    // 默认连接本地127.0.0.1:6000
    TcpClient()
    {
        ips = "127.0.0.1";  // 默认IP
        port = 6000;        // 默认端口
        runing = true;      // 运行状态
        login_status = false;  // 未登录
        op_type = -1;       // 操作类型
        sockfd = -1;
    }

    // 初始化Socket连接
    bool Socket_Init();
    
    // 客户端主循环
    void run();
    
    // 析构函数：关闭Socket连接
    ~TcpClient()
    {
        if (sockfd >= 0)
        {
            close(sockfd);  // 关闭Socket文件描述符
        }
    }

private:
    // 打印菜单并获取用户选择
    void Print_info();
    
    // 用户注册
    void User_Register();
    
    // 用户登录
    void User_Login();
    
    // 查看票务列表
    void Show_Ticket();
    
    // 预约票务
    void YD_Ticket();
    
    // 查看我的预约
    void Show_My_Yuyue();
    
    // 取消预约
    void Cancel_Yuyue();

    // 文件上传
    void Upload_File();
    
    // 辅助函数：发送请求并接收响应
    bool Send_Request(uint8_t cmd, const Json::Value &request, Json::Value &response);
    
    // 辅助函数：接收服务器响应（解析协议）
    bool Receive_Response(Json::Value &response);

    string ips;         // 服务器IP地址
    short port;         // 服务器端口号
    int sockfd;         // Socket文件描述符
    bool runing;        // 客户端是否在运行
    bool login_status;  // 用户登录状态（true=已登录）

    int op_type;        // 当前操作类型
    string username;    // 用户名
    string usertel;     // 用户手机号（账号）
    
    ProtocolHandler m_protocol; // 协议处理器
};
