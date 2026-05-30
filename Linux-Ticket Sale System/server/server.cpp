// 服务器实现文件
//
// 这个文件包含服务器端所有函数的实现
// 主要包括：
// 1. 配置读取
// 2. 数据库操作（MySQL）
// 3. Redis缓存和分布式锁
// 4. Socket事件处理
// 5. TCP服务器主循环

#include "server.h"

// 从配置文件读取服务器配置
// 这个函数会逐行读取配置文件，解析key-value对
bool ServerConfig::ReadConf(string filename)
{
    FILE *fp = fopen(filename.c_str(), "r");
    if (fp == NULL)
    {
        cerr << "配置文件打开失败: " << filename << endl;
        return false;
    }

    char buff[128] = {0};
    int line_num = 1;
    
    while (fgets(buff, 127, fp) != NULL)
    {
        // 跳过注释行和空行
        if (buff[0] == '#' || buff[0] == '\n')
        {
            line_num++;
            continue;
        }

        if (buff[strlen(buff) - 1] == '\n')
        {
            buff[strlen(buff) - 1] = '\0';
        }

        // 使用strtok分割字符串（空格分隔）
        char *key = strtok(buff, " ");
        char *value = strtok(NULL, " ");

        if (key == NULL)
        {
            line_num++;
            continue;
        }

        // 解析各种配置项
        if (strcmp(key, "ip") == 0 && value) listen_ip = value;
        else if (strcmp(key, "port") == 0 && value) listen_port = atoi(value);
        else if (strcmp(key, "lismax") == 0 && value) listen_max = atoi(value);
        else if (strcmp(key, "corethreadnum") == 0 && value) thread_core_num = atoi(value);
        else if (strcmp(key, "maxthreadnum") == 0 && value) thread_max_num = atoi(value);
        else if (strcmp(key, "taskmax") == 0 && value) task_queue_max = atoi(value);
        else if (strcmp(key, "taskwaittime") == 0 && value) task_wait_time_us = atoi(value);
        else if (strcmp(key, "threadwaittime") == 0 && value) thread_wait_time_s = atoi(value);
        else if (strcmp(key, "enablelogclean") == 0 && value) enable_log_clean = atoi(value);
        else if (strcmp(key, "logkeepdays") == 0 && value) log_keep_days = atoi(value);
        else if (strcmp(key, "logdir") == 0 && value) log_dir = value;
        else if (strcmp(key, "dbhost") == 0 && value) db_host = value;
        else if (strcmp(key, "dbport") == 0 && value) db_port = atoi(value);
        else if (strcmp(key, "dbname") == 0 && value) db_name = value;
        else if (strcmp(key, "dbuser") == 0 && value) db_user = value;
        else if (strcmp(key, "dbpassword") == 0 && value) db_password = value;
        else if (strcmp(key, "dbpoolsize") == 0 && value) db_pool_size = atoi(value);
        else if (strcmp(key, "redishost") == 0 && value) redis_host = value;
        else if (strcmp(key, "redisport") == 0 && value) redis_port = atoi(value);
        else if (strcmp(key, "redispassword") == 0) redis_password = value ? value : "";
        else if (strcmp(key, "redispoolsize") == 0 && value) redis_pool_size = atoi(value);
        else if (strcmp(key, "redisttl") == 0 && value) redis_ttl = atoi(value);

        line_num++;
    }

    fclose(fp);
    return true;
}

// 打印配置信息（用于调试）
void ServerConfig::PrintInfo()
{
    LOG_INFO << "========== Server Config ==========";
    LOG_INFO << "Listen IP: " << listen_ip;
    LOG_INFO << "Listen Port: " << listen_port;
    LOG_INFO << "Listen Max: " << listen_max;
    LOG_INFO << "Thread Core Num: " << thread_core_num;
    LOG_INFO << "Thread Max Num: " << thread_max_num;
    LOG_INFO << "Task Queue Max: " << task_queue_max;
    LOG_INFO << "Task Wait Time: " << task_wait_time_us << "us";
    LOG_INFO << "Thread Wait Time: " << thread_wait_time_s << "s";
    LOG_INFO << "Enable Log Clean: " << enable_log_clean;
    LOG_INFO << "Log Keep Days: " << log_keep_days << " days";
    LOG_INFO << "Log Directory: " << log_dir;
    LOG_INFO << "DB Host: " << db_host;
    LOG_INFO << "DB Port: " << db_port;
    LOG_INFO << "DB Name: " << db_name;
    LOG_INFO << "DB User: " << db_user;
    LOG_INFO << "DB Pool Size: " << db_pool_size;
    LOG_INFO << "Redis Host: " << redis_host;
    LOG_INFO << "Redis Port: " << redis_port;
    LOG_INFO << "Redis Pool Size: " << redis_pool_size;
    LOG_INFO << "Redis TTL: " << redis_ttl << "s";
    LOG_INFO << "===================================";
}

// 重置监听Socket的epoll事件
void ListenSocket::ResetEvent()
{
    struct epoll_event ev;
    ev.data.ptr = this;
    ev.events = EPOLLIN | EPOLLONESHOT;
    epoll_ctl(m_epfd, EPOLL_CTL_MOD, m_fd, &ev);
}

// 重置连接Socket的epoll事件
void ConnectSocket::ResetEvent()
{
    struct epoll_event ev;
    ev.data.ptr = this;
    ev.events = EPOLLIN | EPOLLONESHOT;
    epoll_ctl(m_epfd, EPOLL_CTL_MOD, m_fd, &ev);
}

// 用户注册
bool MysqlClient::Db_User_Register(const string &tel, const string &name, const string &passwd)
{
    LOG_INFO << "[DB] 用户注册 - tel: " << tel;
    
    mysql_pool::ConnectionGuard guard;
    if (!guard.valid())
    {
        LOG_ERROR << "获取数据库连接失败";
        return false;
    }
    
    MYSQL* conn = guard.get();

    // 防止SQL注入，转义用户输入
    char escaped_tel[512] = {0};
    char escaped_name[512] = {0};
    char escaped_passwd[512] = {0};

    mysql_real_escape_string(conn, escaped_tel, tel.c_str(), tel.length());
    mysql_real_escape_string(conn, escaped_name, name.c_str(), name.length());
    mysql_real_escape_string(conn, escaped_passwd, passwd.c_str(), passwd.length());

    char sql[2048] = {0};
    snprintf(sql, sizeof(sql), 
        "insert into user_info(UserId, Tel, Name, Passwd, Status, Ztime) values(0,'%s','%s','%s',1,curdate())",
        escaped_tel, escaped_name, escaped_passwd);

    if (mysql_query(conn, sql) != 0)
    {
        LOG_ERROR << "注册失败: " << mysql_error(conn);
        return false;
    }
    LOG_INFO << "[DB] 用户注册成功 - tel: " << tel;
    return true;
}

// 用户登录验证
bool MysqlClient::Db_User_Login(const string &tel, string &name, const string &passwd)
{
    mysql_pool::ConnectionGuard guard;
    if (!guard.valid())
    {
        LOG_ERROR << "获取数据库连接失败";
        return false;
    }
    
    MYSQL* conn = guard.get();

    char escaped_tel[512] = {0};
    char escaped_passwd[512] = {0};

    mysql_real_escape_string(conn, escaped_tel, tel.c_str(), tel.length());
    mysql_real_escape_string(conn, escaped_passwd, passwd.c_str(), passwd.length());

    char sql[2048] = {0};
    snprintf(sql, sizeof(sql), 
        "select Name from user_info where Tel='%s' and Passwd='%s'",
        escaped_tel, escaped_passwd);

    if (mysql_query(conn, sql) != 0)
    {
        return false;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL)
    {
        return false;
    }

    if (mysql_num_rows(result) != 1)
    {
        mysql_free_result(result);
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    name = row[0];
    mysql_free_result(result);
    return true;
}

// 从Redis缓存获取票务列表
bool Cache_Get_Ticket_List(Json::Value &res)
{
    redis_pool::RedisConnectionGuard guard;
    if (!guard.valid()) {
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(guard.get(), "GET ticket:list");
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        if (reply) freeReplyObject(reply);
        return false;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        Json::Reader reader;
        bool success = reader.parse(reply->str, res);
        freeReplyObject(reply);
        return success;
    }

    freeReplyObject(reply);
    return false;
}

// 设置Redis缓存
bool Cache_Set_Ticket_List(const Json::Value &data, int ttl)
{
    redis_pool::RedisConnectionGuard guard;
    if (!guard.valid()) {
        return false;
    }

    Json::FastWriter writer;
    std::string json_str = writer.write(data);

    redisReply* reply = (redisReply*)redisCommand(guard.get(), "SETEX ticket:list %d %s", ttl, json_str.c_str());
    bool success = reply && reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0;
    if (reply) freeReplyObject(reply);
    return success;
}

// 删除Redis缓存
bool Cache_Delete_Ticket_List()
{
    redis_pool::RedisConnectionGuard guard;
    if (!guard.valid()) {
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(guard.get(), "DEL ticket:list");
    bool success = reply && reply->type == REDIS_REPLY_INTEGER;
    if (reply) freeReplyObject(reply);
    return success;
}

// 获取Redis分布式锁
bool Redis_Lock(const std::string &key, int timeout_ms)
{
    redis_pool::RedisConnectionGuard guard;
    if (!guard.valid()) {
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(guard.get(), "SET %s 1 NX PX %d", key.c_str(), timeout_ms);
    bool success = reply && reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0;
    if (reply) freeReplyObject(reply);
    return success;
}

// 释放Redis分布式锁
bool Redis_Unlock(const std::string &key)
{
    redis_pool::RedisConnectionGuard guard;
    if (!guard.valid()) {
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(guard.get(), "DEL %s", key.c_str());
    bool success = reply && reply->type == REDIS_REPLY_INTEGER;
    if (reply) freeReplyObject(reply);
    return success;
}

// 全局配置指针
static ServerConfig* g_config = nullptr;

// 查询所有可预约票务（带缓存）
bool MysqlClient::Db_Show_Ticket(Json::Value &res)
{
    // 先尝试从Redis缓存获取
    if (Cache_Get_Ticket_List(res)) {
        LOG_INFO << "[Redis] 命中缓存: ticket:list";
        return true;
    }

    // 缓存未命中，从MySQL查询
    LOG_INFO << "[Redis] 缓存未命中，从数据库查询";
    mysql_pool::ConnectionGuard guard;
    if (!guard.valid())
    {
        LOG_ERROR << "获取数据库连接失败";
        return false;
    }

    MYSQL* conn = guard.get();

    string sql = "select tk_id, tk_name, tk_max, tk_count, datetime from ticket_table where status=1";
    if (mysql_query(conn, sql.c_str()) != 0)
    {
        return false;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL)
    {
        return false;
    }

    int num = mysql_num_rows(result);
    res["status"] = "OK";
    res["num"] = num;

    if (num == 0)
    {
        mysql_free_result(result);
        return true;
    }

    for (int i = 0; i < num; i++)
    {
        Json::Value tmp;
        MYSQL_ROW row = mysql_fetch_row(result);
        tmp["ticket_id"] = row[0];
        tmp["ticket_name"] = row[1];
        tmp["ticket_max"] = row[2];
        tmp["ticket_count"] = row[3];
        tmp["day_time"] = row[4];
        res["ticket_arr"].append(tmp);
    }
    mysql_free_result(result);

    // 缓存查询结果
    if (g_config) {
        Cache_Set_Ticket_List(res, g_config->redis_ttl);
        LOG_INFO << "[Redis] 缓存已更新: ticket:list, TTL=" << g_config->redis_ttl;
    }

    return true;
}

// 预约票务（事务 + 分布式锁）
bool MysqlClient::Db_Yd_Ticket(string& usertel, string& ticketid)
{
    // 获取分布式锁
    std::string lock_key = "lock:ticket:" + ticketid;
    bool has_lock = Redis_Lock(lock_key, 5000);
    if (!has_lock) {
        LOG_WARN << "[Redis] 获取锁失败: " << lock_key;
        return false;
    }
    LOG_INFO << "[Redis] 获取锁成功: " << lock_key;

    bool result = false;
    mysql_pool::ConnectionGuard guard;
    if (!guard.valid())
    {
        LOG_ERROR << "获取数据库连接失败";
        Redis_Unlock(lock_key);
        return false;
    }

    MYSQL* conn = guard.get();

    mysql_query(conn, "BEGIN");

    char escaped_tel[512] = {0};
    mysql_real_escape_string(conn, escaped_tel, usertel.c_str(), usertel.length());

    // 更新票务库存（乐观锁）
    char sql_update[512] = {0};
    snprintf(sql_update, sizeof(sql_update), 
        "update ticket_table set tk_count = tk_count + 1 where tk_id = %s and tk_count < tk_max and status=1", 
        ticketid.c_str());
    
    if (mysql_query(conn, sql_update) != 0)
    {
        mysql_query(conn, "ROLLBACK");
        Redis_Unlock(lock_key);
        return false;
    }

    if (mysql_affected_rows(conn) == 0)
    {
        mysql_query(conn, "ROLLBACK");
        Redis_Unlock(lock_key);
        return false;
    }

    // 插入预约记录
    char sql_insert[2048] = {0};
    snprintf(sql_insert, sizeof(sql_insert), 
        "insert into yd_table(yd_id, tel, tk_id, ctime, status) values(0,'%s',%s,now(),1)", 
        escaped_tel, ticketid.c_str());
    
    if (mysql_query(conn, sql_insert) != 0)
    {
        mysql_query(conn, "ROLLBACK");
        Redis_Unlock(lock_key);
        return false;
    }

    mysql_query(conn, "COMMIT");
    result = true;

    // 清除缓存
    Cache_Delete_Ticket_List();
    LOG_INFO << "[Redis] 缓存已清除: ticket:list";

    // 释放锁
    Redis_Unlock(lock_key);
    LOG_INFO << "[Redis] 锁已释放: " << lock_key;

    return result;
}

// 查询用户预约列表
bool MysqlClient::Db_Get_Yuyue(string& usertel, Json::Value& res)
{
    mysql_pool::ConnectionGuard guard;
    if (!guard.valid())
    {
        LOG_ERROR << "获取数据库连接失败";
        return false;
    }
    
    MYSQL* conn = guard.get();

    char escaped_tel[512] = {0};
    mysql_real_escape_string(conn, escaped_tel, usertel.c_str(), usertel.length());

    char sql[2048] = {0};
    snprintf(sql, sizeof(sql), 
        "select y.tk_id, t.tk_name, t.datetime, y.ctime from yd_table y "
        "join ticket_table t on y.tk_id = t.tk_id where y.tel = '%s' and y.status=1", 
        escaped_tel);

    if (mysql_query(conn, sql) != 0)
    {
        return false;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL)
    {
        return false;
    }

    int num = mysql_num_rows(result);
    res["status"] = "OK";
    res["num"] = num;

    if (num == 0)
    {
        mysql_free_result(result);
        return true;
    }

    for (int i = 0; i < num; i++)
    {
        Json::Value tmp;
        MYSQL_ROW row = mysql_fetch_row(result);
        tmp["ticket_id"] = row[0];
        tmp["ticket_name"] = row[1];
        tmp["day_time"] = row[2];
        tmp["yuyue_time"] = row[3];
        res["yuyue_arr"].append(tmp);
    }
    mysql_free_result(result);
    return true;
}

// 取消预约（事务 + 分布式锁）
bool MysqlClient::Db_Cancel_Yuyue(string& usertel, string& ticketid)
{
    std::string lock_key = "lock:ticket:" + ticketid;
    bool has_lock = Redis_Lock(lock_key, 5000);
    if (!has_lock) {
        LOG_WARN << "[Redis] 获取锁失败: " << lock_key;
        return false;
    }
    LOG_INFO << "[Redis] 获取锁成功: " << lock_key;

    bool result = false;
    mysql_pool::ConnectionGuard guard;
    if (!guard.valid())
    {
        LOG_ERROR << "获取数据库连接失败";
        Redis_Unlock(lock_key);
        return false;
    }

    MYSQL* conn = guard.get();

    mysql_query(conn, "BEGIN");

    char escaped_tel[512] = {0};
    mysql_real_escape_string(conn, escaped_tel, usertel.c_str(), usertel.length());

    // 更新预约状态为已取消
    char sql_update_yd[2048] = {0};
    snprintf(sql_update_yd, sizeof(sql_update_yd), 
        "update yd_table set status=0 where tel = '%s' and tk_id = %s and status=1", 
        escaped_tel, ticketid.c_str());
    
    if (mysql_query(conn, sql_update_yd) != 0)
    {
        mysql_query(conn, "ROLLBACK");
        Redis_Unlock(lock_key);
        return false;
    }

    if (mysql_affected_rows(conn) == 0)
    {
        mysql_query(conn, "ROLLBACK");
        Redis_Unlock(lock_key);
        return false;
    }

    // 恢复票务库存
    char sql_update_ticket[512] = {0};
    snprintf(sql_update_ticket, sizeof(sql_update_ticket), 
        "update ticket_table set tk_count = tk_count - 1 where tk_id = %s", 
        ticketid.c_str());
    
    if (mysql_query(conn, sql_update_ticket) != 0)
    {
        mysql_query(conn, "ROLLBACK");
        Redis_Unlock(lock_key);
        return false;
    }

    mysql_query(conn, "COMMIT");
    result = true;

    Cache_Delete_Ticket_List();
    LOG_INFO << "[Redis] 缓存已清除: ticket:list";

    Redis_Unlock(lock_key);
    LOG_INFO << "[Redis] 锁已释放: " << lock_key;

    return result;
}

// 静态成员初始化
int ListenSocket::connection_count = 0;

// 处理新连接
void ListenSocket::Handle_Data()
{
    int client_fd = accept(m_fd, NULL, NULL);
    if (client_fd < 0)
    {
        LOG_ERROR << "accept失败";
        return;
    }
    
    ResetEvent();
    LOG_INFO << "新客户端连接 - fd=" << client_fd << ", 当前连接数=" << ++connection_count;

    ConnectSocket *cs = new ConnectSocket(client_fd, m_epfd);
    if (cs == NULL)
    {
        LOG_ERROR << "创建ConnectSocket失败";
        close(client_fd);
        return;
    }

    struct epoll_event ev;
    ev.data.ptr = cs;
    ev.events = EPOLLIN | EPOLLONESHOT;
    
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
    {
        LOG_ERROR << "epoll注册连接Socket失败";
        delete cs;
        return;
    }
}

// 发送成功响应
void ConnectSocket::Send_OK()
{
    Json::Value tmp;
    tmp["status"] = "OK";
    string resp = tmp.toStyledString();
    send(m_fd, resp.c_str(), resp.length(), 0);
}

// 发送失败响应
void ConnectSocket::Send_ERR()
{
    Json::Value tmp;
    tmp["status"] = "ERR";
    string resp = tmp.toStyledString();
    send(m_fd, resp.c_str(), resp.length(), 0);
}

// 发送JSON对象响应
void ConnectSocket::Send_Jsonobj(Json::Value &root)
{
    string resp = root.toStyledString();
    send(m_fd, resp.c_str(), resp.length(), 0);
}

// 解析请求中的操作类型
void ConnectSocket::Get_OpType(char buff[])
{
    Json::Reader reader;
    m_request.clear();
    if (!reader.parse(buff, m_request))
    {
        m_op_type = -1;
        return;
    }
    m_op_type = m_request["type"].asInt();
}

// 处理用户注册请求
void ConnectSocket::User_Register()
{
    LOG_INFO << "[SERVER] 处理注册请求";
    string username = m_request["user_name"].asString();
    string usertel = m_request["user_tel"].asString();
    string passwd = m_request["user_passwd"].asString();

    MysqlClient cli;
    
    if (!cli.Db_User_Register(usertel, username, passwd))
    {
        Send_ERR();
        return;
    }
    
    Send_OK();
}

// 处理用户登录请求
void ConnectSocket::User_Login()
{
    LOG_INFO << "[SERVER] 处理登录请求";
    string usertel = m_request["user_tel"].asString();
    string passwd = m_request["user_passwd"].asString();
    string username;

    MysqlClient cli;

    if (!cli.Db_User_Login(usertel, username, passwd))
    {
        LOG_WARN << "用户登录失败 - tel: " << usertel;
        Send_ERR();
        return;
    }

    LOG_INFO << "用户登录成功 - tel: " << usertel << ", name: " << username;

    Json::Value res;
    res["status"] = "OK";
    res["user_name"] = username;
    Send_Jsonobj(res);
}

// 处理查看票务请求
void ConnectSocket::Show_Ticket()
{
    LOG_INFO << "[SERVER] 处理查看票务请求";
    Json::Value res;
    MysqlClient cli;

    if (!cli.Db_Show_Ticket(res))
    {
        Send_ERR();
        return;
    }
    
    Send_Jsonobj(res);
}

// 处理预约票务请求
void ConnectSocket::Yd_Ticket()
{
    LOG_INFO << "[SERVER] 处理预约请求";
    string usertel = m_request["user_tel"].asString();
    string ticketid = m_request["ticket_id"].asString();

    MysqlClient cli;

    if (!cli.Db_Yd_Ticket(usertel, ticketid))
    {
        Send_ERR();
        return;
    }

    Send_OK();
}

// 处理查看我的预约请求
void ConnectSocket::Show_My_Yuyue()
{
    LOG_INFO << "[SERVER] 处理查看我的预约请求";
    string usertel = m_request["user_tel"].asString();
    Json::Value res;

    MysqlClient cli;

    if (!cli.Db_Get_Yuyue(usertel, res))
    {
        Send_ERR();
        return;
    }

    Send_Jsonobj(res);
}

// 处理取消预约请求
void ConnectSocket::Cancel_Yuyue()
{
    LOG_INFO << "[SERVER] 处理取消预约请求";
    string usertel = m_request["user_tel"].asString();
    string ticketid = m_request["ticket_id"].asString();

    MysqlClient cli;

    if (!cli.Db_Cancel_Yuyue(usertel, ticketid))
    {
        Send_ERR();
        return;
    }

    Send_OK();
}

// 处理客户端数据
void ConnectSocket::Handle_Data()
{
    char buff[1024] = {0};
    int n = recv(m_fd, buff, 1023, 0);
    if (n <= 0)
    {
        LOG_INFO << "客户端断开连接 - fd=" << m_fd;
        delete this;
        return;
    }

    Get_OpType(buff);

    switch (m_op_type)
    {
    case LOGIN: User_Login(); break;
    case REGISTER: User_Register(); break;
    case SHOW_TICKET: Show_Ticket(); break;
    case BOOK_TICKET: Yd_Ticket(); break;
    case MY_BOOKINGS: Show_My_Yuyue(); break;
    case CANCEL_BOOKING: Cancel_Yuyue(); break;
    case EXIT: LOG_INFO << "客户端请求退出"; break;
    default: LOG_WARN << "未知操作类型: " << m_op_type; break;
    }

    ResetEvent();
}

// 服务器初始化
bool TcpServer::Ser_Init()
{
    if (!create_socket())
    {
        return false;
    }

    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd == -1)
    {
        return false;
    }

    return true;
}

// 服务器主循环
void TcpServer::Run()
{
    if (!Ser_Init())
    {
        LOG_ERROR << "服务器初始化失败";
        return;
    }

    ListenSocket *sock = new ListenSocket(m_listen_fd, m_epoll_fd);
    if (sock == NULL)
    {
        LOG_ERROR << "创建监听Socket失败";
        return;
    }

    struct epoll_event ev;
    ev.data.ptr = sock;
    ev.events = EPOLLIN | EPOLLONESHOT;
    
    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_listen_fd, &ev) == -1)
    {
        LOG_ERROR << "epoll注册监听Socket失败";
        return;
    }

    LOG_INFO << "========== 服务器启动成功 ==========";
    LOG_INFO << "监听地址: " << m_config.listen_ip << ":" << m_config.listen_port;

    while (true)
    {
        m_event_count = epoll_wait(m_epoll_fd, m_events, EPOLL_MAX_EVENTS, 5000);
        if (m_event_count == -1)
        {
            LOG_ERROR << "epoll_wait错误";
        }
        else if (m_event_count > 0)
        {
            do_events();
        }
    }
}

// 处理就绪事件
void TcpServer::do_events()
{
    for (int i = 0; i < m_event_count; i++)
    {
        if (m_events[i].events & EPOLLIN)
        {
            Socket *socket_ptr = static_cast<Socket *>(m_events[i].data.ptr);
            m_thread_pool.Excute([socket_ptr]() {
                socket_ptr->Handle_Data();
            });
        }
    }
}

// 创建监听Socket
bool TcpServer::create_socket()
{
    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd == -1)
    {
        return false;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(m_config.listen_port);
    saddr.sin_addr.s_addr = inet_addr(m_config.listen_ip.c_str());

    if (bind(m_listen_fd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    {
        return false;
    }

    if (listen(m_listen_fd, m_config.listen_max) == -1)
    {
        return false;
    }
    
    return true;
}

// main函数：程序入口
int main(int argc, char *argv[])
{
    string conf_path = "../my.conf";
    if (argc > 1)
    {
        conf_path = argv[1];
    }

    ServerConfig conf;
    if (!conf.ReadConf(conf_path))
    {
        cerr << "配置文件读取失败" << endl;
        exit(1);
    }

    g_config = &conf;

    logfile::LoggerManager::getInstance().init(
        "server",
        conf.log_keep_days,
        conf.log_dir,
        conf.enable_log_clean == 1
    );
    
    logfile::LoggerManager::getInstance().start();
    logfile::LoggerManager::getInstance().setLevel(logfile::LOG_LEVEL::INFO);

    LOG_INFO << "加载配置文件: " << conf_path;
    conf.PrintInfo();

    // 初始化MySQL连接池
    LOG_INFO << "初始化MySQL连接池...";
    mysql_pool::MysqlConnectionPool::getInstance().init(
        conf.db_host,
        conf.db_port,
        conf.db_user,
        conf.db_password,
        conf.db_name,
        conf.db_pool_size
    );
    LOG_INFO << "MySQL连接池初始化完成，连接数: " << conf.db_pool_size;

    // 初始化Redis连接池
    LOG_INFO << "初始化Redis连接池...";
    redis_pool::RedisConnectionPool::getInstance().init(
        conf.redis_host,
        conf.redis_port,
        conf.redis_password,
        conf.redis_pool_size
    );
    LOG_INFO << "Redis连接池初始化完成，连接数: " << conf.redis_pool_size;

    TcpServer ser(conf);
    ser.Run();

    logfile::LoggerManager::getInstance().stop();
    exit(0);
}
