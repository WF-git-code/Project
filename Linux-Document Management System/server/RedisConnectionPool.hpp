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

class RedisConnection {
public:
    RedisConnection(redisContext* ctx) : ctx_(ctx), in_use_(false) {}
    ~RedisConnection() {
        if (ctx_) {
            redisFree(ctx_);
        }
    }

    redisContext* get() { return ctx_; }
    bool valid() { return ctx_ && ctx_->err == 0; }
    void set_in_use(bool use) { in_use_ = use; }
    bool in_use() const { return in_use_; }

private:
    redisContext* ctx_;
    bool in_use_;
};

class RedisConnectionPool {
public:
    static RedisConnectionPool& getInstance() {
        static RedisConnectionPool instance;
        return instance;
    }

    void init(const std::string& host, int port, const std::string& password = "", int pool_size = 4) {
        std::lock_guard<std::mutex> lock(mutex_);
        host_ = host;
        port_ = port;
        password_ = password;
        pool_size_ = pool_size;
        running_ = true;

        for (int i = 0; i < pool_size; ++i) {
            auto conn = createConnection();
            if (conn && conn->valid()) {
                connections_.push(std::move(conn));
            }
        }
    }

    std::shared_ptr<RedisConnection> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] {
            return !connections_.empty() || !running_;
        });

        if (!running_) return nullptr;

        auto conn = std::move(connections_.front());
        connections_.pop();
        conn->set_in_use(true);
        return conn;
    }

    void returnConnection(std::shared_ptr<RedisConnection> conn) {
        if (!conn) return;

        std::lock_guard<std::mutex> lock(mutex_);
        conn->set_in_use(false);
        connections_.push(conn);
        cond_.notify_one();
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        cond_.notify_all();
    }

private:
    RedisConnectionPool() : pool_size_(4), running_(false) {}
    ~RedisConnectionPool() { stop(); }

    std::unique_ptr<RedisConnection> createConnection() {
        struct timeval timeout = {2, 0};
        redisContext* ctx = redisConnectWithTimeout(host_.c_str(), port_, timeout);
        
        if (ctx == nullptr || ctx->err != 0) {
            if (ctx) {
                std::cerr << "Redis connect error: " << ctx->errstr << std::endl;
                redisFree(ctx);
            } else {
                std::cerr << "Redis connect error: can't allocate context" << std::endl;
            }
            return nullptr;
        }

        if (!password_.empty()) {
            redisReply* reply = (redisReply*)redisCommand(ctx, "AUTH %s", password_.c_str());
            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                std::cerr << "Redis auth error" << std::endl;
                if (reply) freeReplyObject(reply);
                redisFree(ctx);
                return nullptr;
            }
            freeReplyObject(reply);
        }

        return std::make_unique<RedisConnection>(ctx);
    }

    std::queue<std::shared_ptr<RedisConnection>> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::string host_;
    int port_;
    std::string password_;
    int pool_size_;
    bool running_;
};

class RedisConnectionGuard {
public:
    RedisConnectionGuard() : conn_(nullptr) {
        conn_ = RedisConnectionPool::getInstance().getConnection();
    }

    ~RedisConnectionGuard() {
        if (conn_) {
            RedisConnectionPool::getInstance().returnConnection(conn_);
        }
    }

    redisContext* get() {
        if (conn_) return conn_->get();
        return nullptr;
    }

    bool valid() {
        return conn_ && conn_->valid();
    }

private:
    std::shared_ptr<RedisConnection> conn_;
};

} // namespace redis_pool

#endif // REDIS_CONNECTION_POOL_HPP
