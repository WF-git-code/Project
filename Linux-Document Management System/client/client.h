// 预约系统客户端

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <jsoncpp/json/json.h>

using namespace std;

// 操作类型枚举
enum OpType
{
    LOGIN = 1,
    REGISTER = 2,
    SHOW_TICKET = 3,
    BOOK_TICKET = 4,
    MY_BOOKINGS = 5,
    CANCEL_BOOKING = 6,
    EXIT = 7
};

// TCP客户端类
class TcpClient
{
public:
    TcpClient(string ser_ip, short ser_port)
    {
        ips = ser_ip;
        port = ser_port;
        runing = true;
        login_status = false;
        op_type = -1;
    }

    TcpClient()
    {
        ips = "127.0.0.1";
        port = 6000;
        runing = true;
        login_status = false;
        op_type = -1;
    }

    bool Socket_Init();
    void run();
    ~TcpClient()
    {
        if (sockfd >= 0)
        {
            close(sockfd);
        }
    }

private:
    void Print_info();
    void User_Register();
    void User_Login();
    void Show_Ticket();
    void YD_Ticket();
    void Show_My_Yuyue();
    void Cancel_Yuyue();

    string ips;
    short port;
    int sockfd;
    bool runing;
    bool login_status;

    int op_type;
    string username;
    string usertel;
};
