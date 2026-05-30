// MySQL连接池头文件
//
// 这个文件实现了MySQL连接池，用于管理MySQL数据库连接
// 主要功能：
// 1. 连接复用（减少数据库连接建立开销）
// 2. 线程安全的连接获取和归还
// 3. RAII自动归还连接（ConnectionGuard）
// 4. 单例模式（整个程序只有一个连接池）

#ifndef MYSQL_CONNECTION_POOL_HPP
#define MYSQL_CONNECTION_POOL_HPP

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace mysql_pool
{
    // MySQL连接封装类
    // 这个类封装了一个MySQL连接，记录连接是否被使用
    class MysqlConnection
    {
    public:
        // 构造函数：传入MySQL连接指针
        MysqlConnection(MYSQL* conn) : conn_(conn), in_use_(false) {}
        
        // 析构函数：关闭MySQL连接
        ~MysqlConnection() {
            if (conn_) {
                mysql_close(conn_);  // 使用MySQL C API关闭连接
            }
        }
        
        // 获取原始MySQL连接指针
        MYSQL* get() { return conn_; }
        
        // 检查连接是否正在使用
        bool is_in_use() { return in_use_; }
        
        // 设置连接是否正在使用
        void set_in_use(bool val) { in_use_ = val; }
        
    private:
        MYSQL* conn_;  // MySQL连接指针
        bool in_use_;  // 标记连接是否正在被使用
    };
    
    // MySQL连接池类
    // 这是一个单例类，整个程序只有一个连接池实例
    class MysqlConnectionPool
    {
    public:
        // 获取连接池单例
        // 使用静态局部变量保证线程安全（C++11及以上标准）
        static MysqlConnectionPool& getInstance() {
            static MysqlConnectionPool instance;  // 静态局部变量，只初始化一次
            return instance;
        }
        
        // 初始化连接池
        // 参数：主机地址、端口、用户名、密码、数据库名、连接池大小
        void init(const std::string& host, int port, 
                  const std::string& user, const std::string& passwd, 
                  const std::string& db, int pool_size = 8) {
            std::lock_guard<std::mutex> lock(mutex_);  // 加锁，防止多线程同时初始化
            
            // 保存配置信息
            host_ = host;
            port_ = port;
            user_ = user;
            passwd_ = passwd;
            db_ = db;
            pool_size_ = pool_size;
            
            // 预创建指定数量的连接
            for (int i = 0; i < pool_size; i++) {
                MYSQL* conn = createConnection();  // 创建一个新连接
                if (conn) {
                    connections_.push(std::make_shared<MysqlConnection>(conn));  // 将连接放入队列
                }
            }
        }
        
        // 从连接池获取一个连接
        // 如果没有可用连接，会阻塞等待
        std::shared_ptr<MysqlConnection> getConnection() {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 等待条件：有可用连接 或者 连接池已停止
            cond_.wait(lock, [this] {
                return !connections_.empty() || !running_;
            });
            
            if (!running_) return nullptr;  // 如果连接池已停止，返回空指针
            
            // 从队列头部取出一个连接
            auto conn = connections_.front();
            connections_.pop();
            conn->set_in_use(true);  // 标记连接正在使用
            return conn;
        }
        
        // 归还一个连接到连接池
        void returnConnection(std::shared_ptr<MysqlConnection> conn) {
            if (!conn) return;  // 空指针直接返回
            
            std::lock_guard<std::mutex> lock(mutex_);
            conn->set_in_use(false);  // 标记连接未使用
            connections_.push(conn);  // 将连接放回队列
            cond_.notify_one();       // 通知一个等待的线程
        }
        
        // 停止连接池
        void stop() {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;          // 标记连接池停止
            cond_.notify_all();        // 唤醒所有等待的线程
        }
        
        // 获取当前空闲连接数（用于调试）
        int size() {
            std::lock_guard<std::mutex> lock(mutex_);
            return connections_.size();
        }
        
    private:
        // 私有构造函数（单例模式，禁止外部直接创建）
        MysqlConnectionPool() : pool_size_(8), running_(true) {}
        
        // 私有析构函数
        ~MysqlConnectionPool() { stop(); }
        
        // 禁止拷贝构造和赋值（单例模式）
        MysqlConnectionPool(const MysqlConnectionPool&) = delete;
        MysqlConnectionPool& operator=(const MysqlConnectionPool&) = delete;
        
        // 创建一个新的MySQL连接
        MYSQL* createConnection() {
            MYSQL* conn = mysql_init(nullptr);  // 初始化MySQL连接
            if (!conn) return nullptr;         // 初始化失败
            
            // 设置字符集为UTF-8（防止中文乱码）
            mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8");
            
            // 连接MySQL数据库
            if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(), 
                                  passwd_.c_str(), db_.c_str(), port_, nullptr, 0)) {
                mysql_close(conn);  // 连接失败，关闭连接
                return nullptr;
            }
            
            return conn;  // 连接成功，返回连接指针
        }
        
    private:
        std::string host_;                           // MySQL主机地址
        int port_;                                   // MySQL端口
        std::string user_;                           // MySQL用户名
        std::string passwd_;                         // MySQL密码
        std::string db_;                             // 数据库名称
        int pool_size_;                              // 连接池大小
        bool running_;                               // 连接池是否在运行
        
        std::queue<std::shared_ptr<MysqlConnection>> connections_;  // 连接队列
        std::mutex mutex_;                                          // 互斥锁
        std::condition_variable cond_;                              // 条件变量
    };
    
    // MySQL连接守卫类（RAII模式）
    // 这个类的作用：
    // 1. 构造时自动从连接池获取连接
    // 2. 析构时自动归还连接到连接池
    // 3. 保证连接一定会被归还（即使发生异常）
    class ConnectionGuard
    {
    public:
        // 构造函数：自动获取连接
        ConnectionGuard() : conn_(nullptr) {
            conn_ = MysqlConnectionPool::getInstance().getConnection();
        }
        
        // 析构函数：自动归还连接
        ~ConnectionGuard() {
            if (conn_) {
                MysqlConnectionPool::getInstance().returnConnection(conn_);
            }
        }
        
        // 获取原始MySQL连接指针
        MYSQL* get() {
            if (conn_) return conn_->get();
            return nullptr;
        }
        
        // 检查连接是否有效
        bool valid() { return conn_ && conn_->get(); }
        
    private:
        std::shared_ptr<MysqlConnection> conn_;  // 持有连接的智能指针
    };
}

#endif // MYSQL_CONNECTION_POOL_HPP
