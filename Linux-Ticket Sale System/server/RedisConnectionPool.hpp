// Redis连接池头文件（LRU优化版）
//
// 这个文件实现了Redis连接池，用于管理Redis连接
// 主要功能：
// 1. 连接复用（减少连接建立开销）
// 2. 线程安全的连接获取和归还
// 3. RAII自动归还连接
// 4. 单例模式（整个程序只有一个连接池）
// 5. LRU算法优化连接管理（最近最少使用）
// 6. 连接有效性检查和自动清理

#ifndef REDIS_CONNECTION_POOL_HPP
#define REDIS_CONNECTION_POOL_HPP

#include <iostream>
#include <string>
#include <list>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <hiredis/hiredis.h>

namespace redis_pool {

// Redis连接封装类
// 这个类封装了一个Redis连接，记录连接是否被使用和最后使用时间
class RedisConnection {
public:
    // 构造函数：传入Redis上下文
    RedisConnection(redisContext* ctx) : ctx_(ctx), in_use_(false) {
        // 初始化最后使用时间为当前时间
        last_used_ = std::chrono::steady_clock::now();
    }
    
    // 析构函数：释放Redis连接
    ~RedisConnection() {
        if (ctx_) {
            redisFree(ctx_);  // 使用hiredis库释放连接
        }
    }

    // 获取原始Redis上下文指针
    redisContext* get() { return ctx_; }
    
    // 检查连接是否有效
    bool valid() { 
        if (!ctx_) return false;
        // 简单检查：检查err标志
        if (ctx_->err != 0) return false;
        // 也可以尝试ping命令进行更可靠的检查
        return true;
    }
    
    // 设置连接是否正在使用
    void set_in_use(bool use) { in_use_ = use; }
    
    // 获取连接是否正在使用
    bool in_use() const { return in_use_; }
    
    // 更新最后使用时间（LRU算法需要）
    void update_last_used() {
        last_used_ = std::chrono::steady_clock::now();
    }
    
    // 获取最后使用时间
    std::chrono::steady_clock::time_point last_used() const {
        return last_used_;
    }

private:
    redisContext* ctx_;  // hiredis的Redis上下文
    bool in_use_;        // 标记连接是否正在被使用
    std::chrono::steady_clock::time_point last_used_;  // 最后使用时间（LRU）
};

// Redis连接池类
// 这是一个单例类，整个程序只有一个连接池
// 使用LRU算法管理连接
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
                connections_.push_back(std::move(conn));  // 将连接放入链表
            }
        }
    }

    // 从连接池获取一个连接
    // 如果没有可用连接，会阻塞等待
    // 使用LRU策略：优先返回最近使用过的连接
    std::shared_ptr<RedisConnection> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待条件：有可用连接 或者 连接池已停止
        cond_.wait(lock, [this] {
            // 检查是否有可用连接
            for (auto& conn : connections_) {
                if (!conn->in_use()) return true;
            }
            return !running_;  // 或者连接池停止了
        });

        if (!running_) return nullptr;  // 如果连接池已停止，返回空

        // 先清理掉无效连接
        cleanupInvalidConnections();
        
        // LRU策略：优先返回最近使用过的连接
        // 遍历找到第一个空闲连接，找到后将其移到链表头部（标记为最近使用）
        for (auto it = connections_.begin(); it != connections_.end(); ++it) {
            if (!(*it)->in_use() && (*it)->valid()) {
                auto conn = *it;
                // 找到后，将其从原位置移除并移动到头部
                connections_.erase(it);
                connections_.push_front(conn);
                conn->set_in_use(true);  // 标记连接正在使用
                conn->update_last_used();  // 更新最后使用时间
                return conn;
            }
        }
        
        // 如果没有找到可用连接（虽然wait过了，但避免竞态）
        return nullptr;
    }

    // 归还一个连接到连接池
    void returnConnection(std::shared_ptr<RedisConnection> conn) {
        if (!conn) return;  // 空指针直接返回

        std::lock_guard<std::mutex> lock(mutex_);
        
        // 检查连接是否有效
        if (!conn->valid()) {
            // 连接无效，不归还，直接丢弃（会被智能指针自动释放）
            std::cerr << "Redis连接已失效，不归还到连接池" << std::endl;
            // 尝试创建新连接补充
            auto new_conn = createConnection();
            if (new_conn && new_conn->valid()) {
                connections_.push_back(std::move(new_conn));
            }
            cond_.notify_one();  // 通知一个等待的线程
            return;
        }
        
        conn->set_in_use(false);  // 标记连接未使用
        conn->update_last_used();  // 更新最后使用时间
        
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
    
    // 清理无效连接（LRU辅助函数）
    void cleanupInvalidConnections() {
        auto it = connections_.begin();
        while (it != connections_.end()) {
            if (!(*it)->valid() && !(*it)->in_use()) {
                // 找到无效且空闲的连接，删除
                std::cerr << "清理Redis无效连接" << std::endl;
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 使用list替代queue，便于LRU管理
    std::list<std::shared_ptr<RedisConnection>> connections_;
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
