#include "client.h"

// 初始化socket连接
bool TcpClient::Socket_Init()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        cout << "socket创建失败" << endl;
        return false;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(ips.c_str());

    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    {
        cout << "连接服务器失败" << endl;
        return false;
    }

    cout << "连接服务器成功" << endl;
    return true;
}

// 客户端主循环
void TcpClient::run()
{
    while (runing)
    {
        Print_info();
        switch (op_type)
        {
        case LOGIN: User_Login(); break;
        case REGISTER: User_Register(); break;
        case SHOW_TICKET: Show_Ticket(); break;
        case BOOK_TICKET: YD_Ticket(); break;
        case MY_BOOKINGS: Show_My_Yuyue(); break;
        case CANCEL_BOOKING: Cancel_Yuyue(); break;
        case EXIT:
            runing = false;
            if (sockfd >= 0) close(sockfd);
            cout << "再见！" << endl;
            break;
        default:
            cout << "无效操作" << endl;
            break;
        }
    }
}

// 用户注册
void TcpClient::User_Register()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            用户注册" << endl;
    cout << "+----------------------------------------+" << endl;
    
    cout << "|  手机号: ";
    cin >> usertel;
    cout << "|  用户名: ";
    cin >> username;
    string passwd;
    cout << "|  密码: ";
    cin >> passwd;
    
    Json::Value val;
    val["type"] = REGISTER;
    val["user_tel"] = usertel;
    val["user_name"] = username;
    val["user_passwd"] = passwd;
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
    
    char buff[256] = {0};
    int n = recv(sockfd, buff, 255, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }
    
    Json::Value rval;
    Json::Reader Read;
    if (!Read.parse(buff, rval))
    {
        cout << "JSON解析失败" << endl;
        return;
    }
    
    if (rval["status"].asString() != "OK")
    {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|            注册失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            注册成功" << endl;
    cout << "+----------------------------------------+" << endl;
    login_status = true;
}

// 用户登录
void TcpClient::User_Login()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            用户登录" << endl;
    cout << "+----------------------------------------+" << endl;
    
    cout << "|  手机号: ";
    cin >> usertel;
    cout << "|  密码: ";
    string passwd;
    cin >> passwd;

    Json::Value val;
    val["type"] = LOGIN;
    val["user_tel"] = usertel;
    val["user_passwd"] = passwd;
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    char buff[256] = {0};
    int n = recv(sockfd, buff, 255, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    Json::Value root;
    Json::Reader Read;
    if (!Read.parse(buff, root))
    {
        cout << "JSON解析失败" << endl;
        return;
    }

    if (root["status"].asString() != "OK")
    {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|            登录失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }

    username = root["user_name"].asString();
    login_status = true;
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            登录成功" << endl;
    cout << "|        欢迎，" << username << endl;
    cout << "+----------------------------------------+" << endl;
}

// 显示菜单
void TcpClient::Print_info()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    if (login_status)
    {
        cout << "|        欢迎，" << username << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|  1. 查看票务" << endl;
        cout << "|  2. 预约票务" << endl;
        cout << "|  3. 我的预约" << endl;
        cout << "|  4. 取消预约" << endl;
        cout << "|  5. 退出" << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|  选择 (1-5): ";
        cin >> op_type;
        op_type += 2;  // 用户选1对应枚举值3
    }
    else
    {
        cout << "|        预约系统" << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|  1. 登录" << endl;
        cout << "|  2. 注册" << endl;
        cout << "|  3. 退出" << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|  选择 (1-3): ";
        cin >> op_type;
        if (op_type == 3) op_type = EXIT;
    }
    cout << "+----------------------------------------+" << endl;
}

// 查看票务列表
void TcpClient::Show_Ticket()
{
    Json::Value val;
    val["type"] = SHOW_TICKET;
    string send_str = val.toStyledString();
    send(sockfd, send_str.c_str(), send_str.size(), 0);

    char buff[1024] = {0};
    if (recv(sockfd, buff, 1023, 0) <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }

    if (res_val["status"].asString() != "OK")
    {
        cout << "获取失败" << endl;
        return;
    }

    int num = res_val["num"].asInt();
    if (num == 0)
    {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|       暂无票务" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }

    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            票务列表" << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "| ID | 名称         | 总数 | 已约 | 日期" << endl;
    cout << "+----------------------------------------+" << endl;
    
    for (int i = 0; i < num; i++)
    {
        printf("|%3s | %-13s | %4s | %4s | %s |\n",
               res_val["ticket_arr"][i]["ticket_id"].asCString(),
               res_val["ticket_arr"][i]["ticket_name"].asCString(),
               res_val["ticket_arr"][i]["ticket_max"].asCString(),
               res_val["ticket_arr"][i]["ticket_count"].asCString(),
               res_val["ticket_arr"][i]["day_time"].asCString());
    }
    
    cout << "+----------------------------------------+" << endl;
}

// 预约票务
void TcpClient::YD_Ticket()
{
    Show_Ticket();
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|          输入要预约的票务ID" << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|  ID: ";
    int index;
    cin >> index;
    
    Json::Value val;
    val["type"] = BOOK_TICKET;
    val["user_tel"] = usertel;
    val["ticket_id"] = to_string(index);
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
    
    char buff[128] = {0};
    int n = recv(sockfd, buff, 127, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }
    
    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }
    
    if (res_val["status"].asString() != "OK")
    {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|          预约失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|          预约成功" << endl;
    cout << "+----------------------------------------+" << endl;
}

// 查看我的预约
void TcpClient::Show_My_Yuyue()
{
    Json::Value val;
    val["type"] = MY_BOOKINGS;
    val["user_tel"] = usertel;
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    char buff[1024] = {0};
    int n = recv(sockfd, buff, 1023, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }

    if (res_val["status"].asString() != "OK")
    {
        cout << "获取失败" << endl;
        return;
    }

    int num = res_val["num"].asInt();
    if (num == 0)
    {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|          暂无预约" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }

    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            我的预约" << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "| ID | 名称         | 日期     | 预约时间" << endl;
    cout << "+----------------------------------------+" << endl;
    
    for (int i = 0; i < num; i++)
    {
        printf("|%3s | %-13s | %s | %s |\n",
               res_val["yuyue_arr"][i]["ticket_id"].asCString(),
               res_val["yuyue_arr"][i]["ticket_name"].asCString(),
               res_val["yuyue_arr"][i]["day_time"].asCString(),
               res_val["yuyue_arr"][i]["yuyue_time"].asCString());
    }
    
    cout << "+----------------------------------------+" << endl;
}

// 取消预约
void TcpClient::Cancel_Yuyue()
{
    Show_My_Yuyue();
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|          输入要取消的票务ID" << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|  ID: ";
    int index;
    cin >> index;

    Json::Value val;
    val["type"] = CANCEL_BOOKING;
    val["user_tel"] = usertel;
    val["ticket_id"] = to_string(index);
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    char buff[128] = {0};
    int n = recv(sockfd, buff, 127, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }

    if (res_val["status"].asString() != "OK")
    {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|          取消失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }

    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|          取消成功" << endl;
    cout << "+----------------------------------------+" << endl;
}

int main()
{
    cout << "+----------------------------------------+" << endl;
    cout << "|        预约系统客户端" << endl;
    cout << "+----------------------------------------+" << endl;
    
    TcpClient mycli;
    if (!mycli.Socket_Init())
    {
        cout << "连接失败" << endl;
        return 1;
    }
    mycli.run();
    
    return 0;
}
