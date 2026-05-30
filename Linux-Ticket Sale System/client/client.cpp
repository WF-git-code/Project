// 预约系统客户端实现文件
//
// 这个文件包含TCP客户端类的实现
// 主要流程：
// 1. 连接服务器
// 2. 显示菜单
// 3. 根据用户选择发送请求
// 4. 接收并显示服务器响应

#include "client.h"

// 初始化Socket连接
// 步骤：socket() -> connect()
bool TcpClient::Socket_Init()
{
    // 步骤1：创建Socket
    // 参数1：AF_INET表示IPv4
    // 参数2：SOCK_STREAM表示TCP协议
    // 参数3：0表示默认协议
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        cout << "socket创建失败" << endl;
        return false;
    }

    // 步骤2：设置服务器地址结构
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));  // 先清零
    saddr.sin_family = AF_INET;        // IPv4
    saddr.sin_port = htons(port);      // 端口号（转网络字节序）
    saddr.sin_addr.s_addr = inet_addr(ips.c_str());  // IP地址

    // 步骤3：连接服务器
    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    {
        cout << "连接服务器失败" << endl;
        return false;
    }

    cout << "连接服务器成功" << endl;
    return true;
}

// 客户端主循环
// 不断显示菜单 -> 处理用户选择 -> 循环
void TcpClient::run()
{
    while (runing)
    {
        Print_info();  // 显示菜单，获取用户选择
        switch (op_type)
        {
        case LOGIN: User_Login(); break;
        case REGISTER: User_Register(); break;
        case SHOW_TICKET: Show_Ticket(); break;
        case BOOK_TICKET: YD_Ticket(); break;
        case MY_BOOKINGS: Show_My_Yuyue(); break;
        case CANCEL_BOOKING: Cancel_Yuyue(); break;
        case EXIT:
            runing = false;  // 标记停止运行
            if (sockfd >= 0) close(sockfd);  // 关闭Socket
            cout << "再见！" << endl;
            break;
        default:
            cout << "无效操作" << endl;
            break;
        }
    }
}

// 用户注册
// 步骤：输入信息 -> 发送请求 -> 接收响应
void TcpClient::User_Register()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            用户注册" << endl;
    cout << "+----------------------------------------+" << endl;
    
    // 输入用户信息
    cout << "|  手机号: ";
    cin >> usertel;
    cout << "|  用户名: ";
    cin >> username;
    string passwd;
    cout << "|  密码: ";
    cin >> passwd;
    
    // 构造JSON请求
    Json::Value val;
    val["type"] = REGISTER;            // 操作类型：注册
    val["user_tel"] = usertel;         // 手机号
    val["user_name"] = username;       // 用户名
    val["user_passwd"] = passwd;       // 密码
    
    // 发送请求给服务器
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
    
    // 接收服务器响应
    char buff[256] = {0};
    int n = recv(sockfd, buff, 255, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }
    
    // 解析JSON响应
    Json::Value rval;
    Json::Reader Read;
    if (!Read.parse(buff, rval))
    {
        cout << "JSON解析失败" << endl;
        return;
    }
    
    // 判断是否成功
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
    login_status = true;  // 注册成功，自动登录
}

// 用户登录
// 步骤：输入手机号和密码 -> 发送请求 -> 接收响应
void TcpClient::User_Login()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            用户登录" << endl;
    cout << "+----------------------------------------+" << endl;
    
    // 输入登录信息
    cout << "|  手机号: ";
    cin >> usertel;
    cout << "|  密码: ";
    string passwd;
    cin >> passwd;

    // 构造JSON请求
    Json::Value val;
    val["type"] = LOGIN;
    val["user_tel"] = usertel;
    val["user_passwd"] = passwd;
    
    // 发送请求
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    // 接收响应
    char buff[256] = {0};
    int n = recv(sockfd, buff, 255, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    // 解析JSON
    Json::Value root;
    Json::Reader Read;
    if (!Read.parse(buff, root))
    {
        cout << "JSON解析失败" << endl;
        return;
    }

    // 判断是否成功
    if (root["status"].asString() != "OK")
    {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|            登录失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }

    // 登录成功！保存用户名
    username = root["user_name"].asString();
    login_status = true;
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            登录成功" << endl;
    cout << "|        欢迎，" << username << endl;
    cout << "+----------------------------------------+" << endl;
}

// 显示菜单并获取用户选择
// 根据登录状态显示不同菜单
void TcpClient::Print_info()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    if (login_status)
    {
        // 已登录：显示业务菜单
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
        op_type += 2;  // 用户选1对应枚举值3（SHOW_TICKET）
    }
    else
    {
        // 未登录：显示登录/注册菜单
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
// 步骤：发送请求 -> 接收响应 -> 显示列表
void TcpClient::Show_Ticket()
{
    // 构造JSON请求（只需type）
    Json::Value val;
    val["type"] = SHOW_TICKET;
    string send_str = val.toStyledString();
    send(sockfd, send_str.c_str(), send_str.size(), 0);

    // 接收响应
    char buff[1024] = {0};
    if (recv(sockfd, buff, 1023, 0) <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    // 解析JSON
    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }

    // 判断是否成功
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

    // 显示票务列表
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
// 步骤：先显示列表 -> 输入ID -> 发送请求
void TcpClient::YD_Ticket()
{
    Show_Ticket();  // 先显示票务列表供用户选择
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|          输入要预约的票务ID" << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|  ID: ";
    int index;
    cin >> index;
    
    // 构造JSON请求
    Json::Value val;
    val["type"] = BOOK_TICKET;
    val["user_tel"] = usertel;
    val["ticket_id"] = to_string(index);
    
    // 发送请求
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
    
    // 接收响应
    char buff[128] = {0};
    int n = recv(sockfd, buff, 127, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }
    
    // 解析JSON
    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }
    
    // 判断是否成功
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
// 步骤：发送请求 -> 接收响应 -> 显示列表
void TcpClient::Show_My_Yuyue()
{
    // 构造JSON请求
    Json::Value val;
    val["type"] = MY_BOOKINGS;
    val["user_tel"] = usertel;
    
    // 发送请求
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    // 接收响应
    char buff[1024] = {0};
    int n = recv(sockfd, buff, 1023, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    // 解析JSON
    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }

    // 判断是否成功
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

    // 显示我的预约列表
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
// 步骤：先显示我的预约 -> 输入ID -> 发送请求
void TcpClient::Cancel_Yuyue()
{
    Show_My_Yuyue();  // 先显示我的预约列表
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|          输入要取消的票务ID" << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|  ID: ";
    int index;
    cin >> index;

    // 构造JSON请求
    Json::Value val;
    val["type"] = CANCEL_BOOKING;
    val["user_tel"] = usertel;
    val["ticket_id"] = to_string(index);
    
    // 发送请求
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    // 接收响应
    char buff[128] = {0};
    int n = recv(sockfd, buff, 127, 0);
    if (n <= 0)
    {
        cout << "服务器断开" << endl;
        return;
    }

    // 解析JSON
    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }

    // 判断是否成功
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

// main函数：程序入口
int main()
{
    cout << "+----------------------------------------+" << endl;
    cout << "|        预约系统客户端" << endl;
    cout << "+----------------------------------------+" << endl;
    
    // 创建客户端对象
    TcpClient mycli;
    if (!mycli.Socket_Init())
    {
        cout << "连接失败" << endl;
        return 1;
    }
    mycli.run();  // 运行客户端主循环
    
    return 0;
}
