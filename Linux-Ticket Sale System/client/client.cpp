// 预约系统客户端实现文件
//
// 这个文件包含TCP客户端类的实现
// 主要流程：
// 1. 连接服务器
// 2. 显示菜单
// 3. 根据用户选择发送请求（使用自定义协议）
// 4. 接收并显示服务器响应
// 5. 文件上传功能（支持大文件和断点续传）

#include "client.h"

// 辅助函数：获取文件大小
static size_t get_file_size(const string &filename)
{
    struct stat st;
    if (stat(filename.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

// 辅助函数：从文件名提取基础名称
static string get_file_basename(const string &filepath)
{
    size_t pos = filepath.find_last_of("/");
    if (pos != string::npos) {
        return filepath.substr(pos + 1);
    }
    return filepath;
}

// 辅助函数：接收服务器响应（解析协议）
bool TcpClient::Receive_Response(Json::Value &response)
{
    char buff[4096] = {0};
    int n = recv(sockfd, buff, 4095, 0);
    if (n <= 0) {
        cout << "服务器断开" << endl;
        return false;
    }

    // 使用协议解析
    std::string data;
    uint8_t cmd;
    if (m_protocol.receive_and_parse(buff, n, data, cmd)) {
        Json::Reader reader;
        if (!reader.parse(data, response)) {
            cout << "JSON解析失败" << endl;
            return false;
        }
        return true;
    }
    cout << "协议解析失败" << endl;
    return false;
}

// 辅助函数：发送请求并接收响应
bool TcpClient::Send_Request(uint8_t cmd, const Json::Value &request, Json::Value &response)
{
    // 使用协议打包
    string packet = ProtocolHandler::pack(cmd, request.toStyledString());
    
    // 发送
    if (send(sockfd, packet.c_str(), packet.size(), 0) <= 0) {
        cout << "发送失败" << endl;
        return false;
    }
    
    // 接收响应
    return Receive_Response(response);
}

// 初始化Socket连接
// 步骤：socket() -> connect()
bool TcpClient::Socket_Init()
{
    // 步骤1：创建Socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        cout << "socket创建失败" << endl;
        return false;
    }

    // 步骤2：设置服务器地址结构
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(ips.c_str());

    // 步骤3：连接服务器
    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        cout << "连接服务器失败" << endl;
        return false;
    }

    cout << "连接服务器成功" << endl;
    return true;
}

// 客户端主循环
void TcpClient::run()
{
    while (runing) {
        Print_info();
        switch (op_type) {
        case LOGIN: User_Login(); break;
        case REGISTER: User_Register(); break;
        case SHOW_TICKET: Show_Ticket(); break;
        case BOOK_TICKET: YD_Ticket(); break;
        case MY_BOOKINGS: Show_My_Yuyue(); break;
        case CANCEL_BOOKING: Cancel_Yuyue(); break;
        case UPLOAD_FILE: Upload_File(); break;
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
    
    Json::Value request;
    request["type"] = REGISTER;
    request["user_tel"] = usertel;
    request["user_name"] = username;
    request["user_passwd"] = passwd;
    
    Json::Value response;
    if (!Send_Request(CMD_REGISTER, request, response)) {
        return;
    }
    
    if (response["status"].asString() != "OK") {
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
    
    // 等待用户按回车继续，避免立即进入下一个操作
    cout << "|  按回车继续...";
    cin.ignore();  // 忽略之前的换行符
    cin.get();     // 等待用户输入
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
    string passwd;
    cout << "|  密码: ";
    cin >> passwd;

    Json::Value request;
    request["type"] = LOGIN;
    request["user_tel"] = usertel;
    request["user_passwd"] = passwd;
    
    Json::Value response;
    if (!Send_Request(CMD_LOGIN, request, response)) {
        return;
    }

    if (response["status"].asString() != "OK") {
        cout << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|            登录失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }

    username = response["user_name"].asString();
    login_status = true;
    
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|            登录成功" << endl;
    cout << "|        欢迎，" << username << endl;
    cout << "+----------------------------------------+" << endl;
    
    // 等待用户按回车继续，避免立即进入下一个操作
    cout << "|  按回车继续...";
    cin.ignore();  // 忽略之前的换行符
    cin.get();     // 等待用户输入
}

// 显示菜单并获取用户选择
void TcpClient::Print_info()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    if (login_status) {
        cout << "|        欢迎，" << username << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|  1. 查看票务" << endl;
        cout << "|  2. 预约票务" << endl;
        cout << "|  3. 我的预约" << endl;
        cout << "|  4. 取消预约" << endl;
        cout << "|  5. 上传文件" << endl;
        cout << "|  6. 退出" << endl;
        cout << "+----------------------------------------+" << endl;
        cout << "|  选择 (1-6): ";
        cin >> op_type;
        if (op_type >= 1 && op_type <= 4) {
            op_type += 2;  // 1->3, 2->4, 3->5, 4->6
        } else if (op_type == 5) {
            op_type = UPLOAD_FILE;
        } else if (op_type == 6) {
            op_type = EXIT;
        }
    } else {
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
    Json::Value request;
    request["type"] = SHOW_TICKET;
    
    Json::Value response;
    if (!Send_Request(CMD_SHOW_TICKET, request, response)) {
        return;
    }

    if (response["status"].asString() != "OK") {
        cout << "获取失败" << endl;
        return;
    }

    int num = response["num"].asInt();
    if (num == 0) {
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
    
    for (int i = 0; i < num; i++) {
        printf("|%3s | %-13s | %4s | %4s | %s |\n",
               response["ticket_arr"][i]["ticket_id"].asCString(),
               response["ticket_arr"][i]["ticket_name"].asCString(),
               response["ticket_arr"][i]["ticket_max"].asCString(),
               response["ticket_arr"][i]["ticket_count"].asCString(),
               response["ticket_arr"][i]["day_time"].asCString());
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
    
    Json::Value request;
    request["type"] = BOOK_TICKET;
    request["user_tel"] = usertel;
    request["ticket_id"] = to_string(index);
    
    Json::Value response;
    if (!Send_Request(CMD_ORDER, request, response)) {
        return;
    }
    
    if (response["status"].asString() != "OK") {
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
    Json::Value request;
    request["type"] = MY_BOOKINGS;
    request["user_tel"] = usertel;
    
    Json::Value response;
    if (!Send_Request(CMD_SHOW_ORDER, request, response)) {
        return;
    }

    if (response["status"].asString() != "OK") {
        cout << "获取失败" << endl;
        return;
    }

    int num = response["num"].asInt();
    if (num == 0) {
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
    
    for (int i = 0; i < num; i++) {
        printf("|%3s | %-13s | %s | %s |\n",
               response["yuyue_arr"][i]["ticket_id"].asCString(),
               response["yuyue_arr"][i]["ticket_name"].asCString(),
               response["yuyue_arr"][i]["day_time"].asCString(),
               response["yuyue_arr"][i]["yuyue_time"].asCString());
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

    Json::Value request;
    request["type"] = CANCEL_BOOKING;
    request["user_tel"] = usertel;
    request["ticket_id"] = to_string(index);
    
    Json::Value response;
    if (!Send_Request(CMD_CANCEL, request, response)) {
        return;
    }

    if (response["status"].asString() != "OK") {
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

// 文件上传（支持大文件和断点续传）
void TcpClient::Upload_File()
{
    cout << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|          文件上传" << endl;
    cout << "+----------------------------------------+" << endl;
    cout << "|  请输入文件路径: ";
    
    string filepath;
    cin.ignore();  // 忽略前面的换行
    getline(cin, filepath);
    
    // 检查文件是否存在
    size_t file_size = get_file_size(filepath);
    if (file_size == 0) {
        cout << "| 文件不存在或为空" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    string filename = get_file_basename(filepath);
    cout << "| 文件名: " << filename << endl;
    cout << "| 文件大小: " << file_size << " 字节" << endl;
    
    // 步骤1：检查断点
    Json::Value check_req;
    check_req["file_name"] = filename;
    Json::Value check_res;
    if (!Send_Request(CMD_FILE_CHECK, check_req, check_res)) {
        cout << "| 检查断点失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    size_t offset = 0;
    bool file_exists = check_res["exists"].asBool();
    if (file_exists) {
        offset = check_res["offset"].asUInt64();
        cout << "| 发现断点，从偏移 " << offset << " 继续上传" << endl;
    } else {
        cout << "| 新文件，从开头开始上传" << endl;
    }
    
    // 步骤2：开始上传
    Json::Value start_req;
    start_req["file_name"] = filename;
    start_req["file_size"] = (Json::UInt64)file_size;
    Json::Value start_res;
    if (!Send_Request(CMD_FILE_START, start_req, start_res)) {
        cout << "| 开始上传失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    if (start_res["status"].asString() != "OK") {
        cout << "| 开始上传失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    // 再次确认offset（服务器可能有不同的偏移）
    offset = start_res["offset"].asUInt64();
    
    // 打开文件
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        cout << "| 无法打开文件" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    // 移动到指定偏移
    if (offset > 0) {
        lseek(fd, offset, SEEK_SET);
    }
    
    // 步骤3：分块上传
    const size_t CHUNK_SIZE = 8192;  // 每块8KB
    char chunk[CHUNK_SIZE];
    bool success = true;
    size_t total_sent = offset;
    
    cout << "| 上传进度: ";
    cout.flush();
    
    while (offset < file_size) {
        // 读取文件块
        ssize_t read_size = read(fd, chunk, CHUNK_SIZE);
        if (read_size <= 0) break;
        
        // 构造请求
        Json::Value chunk_req;
        chunk_req["offset"] = (Json::UInt64)offset;
        chunk_req["data"] = string(chunk, read_size);
        
        Json::Value chunk_res;
        if (!Send_Request(CMD_FILE_CHUNK, chunk_req, chunk_res)) {
            success = false;
            break;
        }
        
        if (chunk_res["status"].asString() != "OK") {
            success = false;
            break;
        }
        
        offset += read_size;
        total_sent += read_size;
        
        // 显示进度
        int percent = (int)((offset * 100) / file_size);
        cout << "\r| 上传进度: " << percent << "%";
        cout.flush();
    }
    
    cout << endl;
    close(fd);
    
    if (!success) {
        cout << "| 上传过程中失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    // 步骤4：结束上传
    Json::Value end_req;  // 空请求
    Json::Value end_res;
    if (!Send_Request(CMD_FILE_END, end_req, end_res)) {
        cout << "| 结束上传失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    if (end_res["status"].asString() != "OK") {
        cout << "| 上传失败" << endl;
        cout << "+----------------------------------------+" << endl;
        return;
    }
    
    cout << "+----------------------------------------+" << endl;
    cout << "|          上传成功!" << endl;
    cout << "| 文件: " << end_res["file_name"].asString() << endl;
    cout << "| 大小: " << end_res["file_size"].asUInt64() << " 字节" << endl;
    cout << "+----------------------------------------+" << endl;
}

// main函数：程序入口
int main()
{
    cout << "+----------------------------------------+" << endl;
    cout << "|        预约系统客户端" << endl;
    cout << "+----------------------------------------+" << endl;
    
    TcpClient mycli;
    if (!mycli.Socket_Init()) {
        cout << "连接失败" << endl;
        return 1;
    }
    mycli.run();
    
    return 0;
}
