// MySQL连接池头文件（LRU优化版）
//
// 这个文件实现了MySQL连接池，用于管理MySQL数据库连接
// 主要功能：
// 1. 连接复用（减少数据库连接建立开销）
// 2. 线程安全的连接获取和归还
// 3. RAII自动归还连接（ConnectionGuard）
// 4. 单例模式（整个程序只有一个连接池）
// 5. LRU算法优化连接管理（最近最少使用）
// 6. 连接有效性检查和自动清理

#ifndef MYSQL_CONNECTION_POOL_HPP
#define MYSQL_CONNECTION_POOL_HPP

#include <mysql/mysql.h>
#include <string>
#include <list>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

namespace mysql_pool
{
    // MySQL连接封装类
    // 这个类封装了一个MySQL连接，记录连接是否被使用和最后使用时间
    class MysqlConnection
    {
    public:
        // 构造函数：传入MySQL连接指针
        MysqlConnection(MYSQL* conn) : conn_(conn), in_use_(false) {
            // 初始化最后使用时间为当前时间（LRU算法需要）
            last_used_ = std::chrono::steady_clock::now();
        }
        
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
        
        // 检查连接是否有效
        bool valid() {
            if (!conn_) return false;
            // 使用ping命令检查连接有效性
            return mysql_ping(conn_) == 0;
        }
        
        // 更新最后使用时间（LRU算法需要）
        void update_last_used() {
            last_used_ = std::chrono::steady_clock::now();
        }
        
        // 获取最后使用时间
        std::chrono::steady_clock::time_point last_used() const {
            return last_used_;
        }
        
    private:
        MYSQL* conn_;  // MySQL连接指针
        bool in_use_;  // 标记连接是否正在被使用
        std::chrono::steady_clock::time_point last_used_;  // 最后使用时间（LRU）
    };
    
    // MySQL连接池类
    // 这是一个单例类，整个程序只有一个连接池实例
    // 使用LRU算法管理连接
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
                    connections_.push_back(std::make_shared<MysqlConnection>(conn));  // 将连接放入链表
                }
            }
        }
        
        // 从连接池获取一个连接
        // 如果没有可用连接，会阻塞等待
        // 使用LRU策略：优先返回最近使用过的连接
        std::shared_ptr<MysqlConnection> getConnection() {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 等待条件：有可用连接 或者 连接池已停止
            cond_.wait(lock, [this] {
                // 检查是否有空闲连接
                for (auto& conn : connections_) {
                    if (!conn->is_in_use()) return true;
                }
                return !running_;
            });
            
            if (!running_) return nullptr;  // 如果连接池已停止，返回空指针
            
            // 先清理无效连接
            cleanupInvalidConnections();
            
            // LRU策略：遍历找到第一个空闲且有效的连接
            for (auto it = connections_.begin(); it != connections_.end(); ++it) {
                if (!(*it)->is_in_use() && (*it)->valid()) {
                    auto conn = *it;
                    
                    // LRU策略：把取出来的连接移到链表头部（标记为最近使用）
                    connections_.erase(it);
                    connections_.push_front(conn);
                    
                    conn->set_in_use(true);     // 标记连接正在使用
                    conn->update_last_used();   // 更新最后使用时间
                    return conn;
                }
            }
            
            // 如果没有找到可用连接（虽然wait过了，但避免竞态）
            return nullptr;
        }
        
        // 归还一个连接到连接池
        void returnConnection(std::shared_ptr<MysqlConnection> conn) {
            if (!conn) return;  // 空指针直接返回
            
            std::lock_guard<std::mutex> lock(mutex_);
            
            // 检查连接是否有效
            if (!conn->valid()) {
                // 连接无效，不归还，直接丢弃
                std::cerr << "MySQL连接已失效，不归还到连接池" << std::endl;
                // 尝试创建新连接补充
                MYSQL* new_conn = createConnection();
                if (new_conn) {
                    connections_.push_back(std::make_shared<MysqlConnection>(new_conn));
                }
                cond_.notify_one();  // 通知一个等待的线程
                return;
            }
            
            conn->set_in_use(false);    // 标记连接未使用
            conn->update_last_used();   // 更新最后使用时间
            
            // LRU策略：将归还的连接放到链表头部（最近使用）
            connections_.push_front(conn);
            
            cond_.notify_one();  // 通知一个等待的线程
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
            
            // 设置连接超时（5秒）
            struct timeval timeout = {5, 0};
            mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
            
            // 连接MySQL数据库
            if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(), 
                                  passwd_.c_str(), db_.c_str(), port_, nullptr, 0)) {
                mysql_close(conn);  // 连接失败，关闭连接
                return nullptr;
            }
            
            return conn;  // 连接成功，返回连接指针
        }
        
        // 清理无效连接（LRU辅助函数）
        void cleanupInvalidConnections() {
            auto it = connections_.begin();
            while (it != connections_.end()) {
                if (!(*it)->valid() && !(*it)->is_in_use()) {
                    // 找到无效且空闲的连接，删除
                    std::cerr << "清理MySQL无效连接" << std::endl;
                    it = connections_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
    private:
        std::string host_;                           // MySQL主机地址
        int port_;                                   // MySQL端口
        std::string user_;                           // MySQL用户名
        std::string passwd_;                         // MySQL密码
        std::string db_;                             // 数据库名称
        int pool_size_;                              // 连接池大小
        bool running_;                               // 连接池是否在运行
        
        std::list<std::shared_ptr<MysqlConnection>> connections_;  // 连接链表（LRU）
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
        bool valid() { 
            return conn_ && conn_->valid(); 
        }
        
    private:
        std::shared_ptr<MysqlConnection> conn_;  // 持有连接的智能指针
    };
}

#endif // MYSQL_CONNECTION_POOL_HPP
