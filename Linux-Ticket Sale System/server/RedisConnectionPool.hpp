// Redis连接池头文件
//
// 这个文件实现了Redis连接池，用于管理Redis连接
// 主要功能：
// 1. 连接复用（减少连接建立开销）
// 2. 线程安全的连接获取和归还
// 3. RAII自动归还连接
// 4. 单例模式（整个程序只有一个连接池）

#ifndef REDIS_CONNECTION_POOL_HPP
#define REDIS_CONNECTION_POOL_HPP

#include <iostream>
#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <hiredis/hiredis.h>

namespace redis_pool {

// Redis连接封装类
// 这个类封装了一个Redis连接，记录连接是否被使用
class RedisConnection {
public:
    // 构造函数：传入Redis上下文
    RedisConnection(redisContext* ctx) : ctx_(ctx), in_use_(false) {}
    
    // 析构函数：释放Redis连接
    ~RedisConnection() {
        if (ctx_) {
            redisFree(ctx_);  // 使用hiredis库释放连接
        }
    }

    // 获取原始Redis上下文指针
    redisContext* get() { return ctx_; }
    
    // 检查连接是否有效
    bool valid() { return ctx_ && ctx_->err == 0; }
    
    // 设置连接是否正在使用
    void set_in_use(bool use) { in_use_ = use; }
    
    // 获取连接是否正在使用
    bool in_use() const { return in_use_; }

private:
    redisContext* ctx_;  // hiredis的Redis上下文
    bool in_use_;        // 标记连接是否正在被使用
};

// Redis连接池类
// 这是一个单例类，整个程序只有一个连接池
class RedisConnectionPool {
public:
    // 获取连接池单例
    // 使用静态局部变量保证线程安全（C++11及以上）
    static RedisConnectionPool& getInstance() {
        static RedisConnectionPool instance;  // 静态局部变量，只初始化一次
        return instance;
    }

    // 初始化连接池
    // 参数：主机地址、端口、密码、连接池大小
    void init(const std::string& host, int port, const std::string& password = "", int pool_size = 4) {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁，防止多线程同时初始化
        
        // 保存配置
        host_ = host;
        port_ = port;
        password_ = password;
        pool_size_ = pool_size;
        running_ = true;  // 标记连接池正在运行

        // 预创建指定数量的连接
        for (int i = 0; i < pool_size; ++i) {
            auto conn = createConnection();  // 创建一个新连接
            if (conn && conn->valid()) {
                connections_.push(std::move(conn));  // 将连接放入队列
            }
        }
    }

    // 从连接池获取一个连接
    // 如果没有可用连接，会阻塞等待
    std::shared_ptr<RedisConnection> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待条件：有可用连接 或者 连接池已停止
        cond_.wait(lock, [this] {
            return !connections_.empty() || !running_;
        });

        if (!running_) return nullptr;  // 如果连接池已停止，返回空

        // 从队列头部取出一个连接
        auto conn = std::move(connections_.front());
        connections_.pop();
        conn->set_in_use(true);  // 标记连接正在使用
        return conn;
    }

    // 归还一个连接到连接池
    void returnConnection(std::shared_ptr<RedisConnection> conn) {
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

private:
    // 私有构造函数（单例模式）
    RedisConnectionPool() : pool_size_(4), running_(false) {}
    
    // 私有析构函数（单例模式）
    ~RedisConnectionPool() { stop(); }

    // 创建一个新的Redis连接
    std::unique_ptr<RedisConnection> createConnection() {
        // 设置连接超时时间：2秒
        struct timeval timeout = {2, 0};
        
        // 连接Redis
        redisContext* ctx = redisConnectWithTimeout(host_.c_str(), port_, timeout);
        
        // 检查连接是否成功
        if (ctx == nullptr || ctx->err != 0) {
            if (ctx) {
                std::cerr << "Redis connect error: " << ctx->errstr << std::endl;
                redisFree(ctx);
            } else {
                std::cerr << "Redis connect error: can't allocate context" << std::endl;
            }
            return nullptr;
        }

        // 如果设置了密码，进行认证
        if (!password_.empty()) {
            redisReply* reply = (redisReply*)redisCommand(ctx, "AUTH %s", password_.c_str());
            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                std::cerr << "Redis auth error" << std::endl;
                if (reply) freeReplyObject(reply);  // 释放reply对象
                redisFree(ctx);                    // 释放连接
                return nullptr;
            }
            freeReplyObject(reply);  // 认证成功，释放reply
        }

        // 返回封装好的连接对象
        return std::make_unique<RedisConnection>(ctx);
    }

    std::queue<std::shared_ptr<RedisConnection>> connections_;  // 连接队列
    std::mutex mutex_;                                          // 互斥锁
    std::condition_variable cond_;                              // 条件变量
    std::string host_;                                          // Redis主机地址
    int port_;                                                  // Redis端口
    std::string password_;                                      // Redis密码
    int pool_size_;                                             // 连接池大小
    bool running_;                                              // 连接池是否在运行
};

// Redis连接守卫类（RAII模式）
// 这个类的作用：
// 1. 构造时自动从连接池获取连接
// 2. 析构时自动归还连接到连接池
// 3. 保证连接一定会被归还（即使发生异常）
class RedisConnectionGuard {
public:
    // 构造函数：自动获取连接
    RedisConnectionGuard() : conn_(nullptr) {
        conn_ = RedisConnectionPool::getInstance().getConnection();
    }

    // 析构函数：自动归还连接
    ~RedisConnectionGuard() {
        if (conn_) {
            RedisConnectionPool::getInstance().returnConnection(conn_);
        }
    }

    // 获取原始Redis上下文
    redisContext* get() {
        if (conn_) return conn_->get();
        return nullptr;
    }

    // 检查连接是否有效
    bool valid() {
        return conn_ && conn_->valid();
    }

private:
    std::shared_ptr<RedisConnection> conn_;  // 持有连接的智能指针
};

} // namespace redis_pool

#endif // REDIS_CONNECTION_POOL_HPP
