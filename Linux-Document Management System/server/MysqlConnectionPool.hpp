// MySQL连接池 - 简单实现，学生求职风格
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
    // 封装MySQL连接的RAII类
    class MysqlConnection
    {
    public:
        MysqlConnection(MYSQL* conn) : conn_(conn), in_use_(false) {}
        ~MysqlConnection() {
            if (conn_) {
                mysql_close(conn_);
            }
        }
        
        MYSQL* get() { return conn_; }
        bool is_in_use() { return in_use_; }
        void set_in_use(bool val) { in_use_ = val; }
        
    private:
        MYSQL* conn_;
        bool in_use_;
    };
    
    // 连接池类
    class MysqlConnectionPool
    {
    public:
        // 获取单例
        static MysqlConnectionPool& getInstance() {
            static MysqlConnectionPool instance;
            return instance;
        }
        
        // 初始化连接池
        void init(const std::string& host, int port, 
                  const std::string& user, const std::string& passwd, 
                  const std::string& db, int pool_size = 8) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            host_ = host;
            port_ = port;
            user_ = user;
            passwd_ = passwd;
            db_ = db;
            pool_size_ = pool_size;
            
            // 初始化连接
            for (int i = 0; i < pool_size; i++) {
                MYSQL* conn = createConnection();
                if (conn) {
                    connections_.push(std::make_shared<MysqlConnection>(conn));
                }
            }
        }
        
        // 获取连接
        std::shared_ptr<MysqlConnection> getConnection() {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 等待有可用连接
            cond_.wait(lock, [this] {
                return !connections_.empty() || !running_;
            });
            
            if (!running_) return nullptr;
            
            auto conn = connections_.front();
            connections_.pop();
            conn->set_in_use(true);
            return conn;
        }
        
        // 归还连接
        void returnConnection(std::shared_ptr<MysqlConnection> conn) {
            if (!conn) return;
            
            std::lock_guard<std::mutex> lock(mutex_);
            conn->set_in_use(false);
            connections_.push(conn);
            cond_.notify_one();
        }
        
        // 停止连接池
        void stop() {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            cond_.notify_all();
        }
        
        // 获取当前连接数（用于调试）
        int size() {
            std::lock_guard<std::mutex> lock(mutex_);
            return connections_.size();
        }
        
    private:
        MysqlConnectionPool() : pool_size_(8), running_(true) {}
        ~MysqlConnectionPool() { stop(); }
        
        // 禁止拷贝
        MysqlConnectionPool(const MysqlConnectionPool&) = delete;
        MysqlConnectionPool& operator=(const MysqlConnectionPool&) = delete;
        
        // 创建一个新连接
        MYSQL* createConnection() {
            MYSQL* conn = mysql_init(nullptr);
            if (!conn) return nullptr;
            
            mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8");
            
            if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(), 
                                   passwd_.c_str(), db_.c_str(), port_, nullptr, 0)) {
                mysql_close(conn);
                return nullptr;
            }
            
            return conn;
        }
        
    private:
        std::string host_;
        int port_;
        std::string user_;
        std::string passwd_;
        std::string db_;
        int pool_size_;
        bool running_;
        
        std::queue<std::shared_ptr<MysqlConnection>> connections_;
        std::mutex mutex_;
        std::condition_variable cond_;
    };
    
    // RAII连接获取器，自动归还
    class ConnectionGuard
    {
    public:
        ConnectionGuard() : conn_(nullptr) {
            conn_ = MysqlConnectionPool::getInstance().getConnection();
        }
        
        ~ConnectionGuard() {
            if (conn_) {
                MysqlConnectionPool::getInstance().returnConnection(conn_);
            }
        }
        
        MYSQL* get() {
            if (conn_) return conn_->get();
            return nullptr;
        }
        
        bool valid() { return conn_ && conn_->get(); }
        
    private:
        std::shared_ptr<MysqlConnection> conn_;
    };
}

#endif
