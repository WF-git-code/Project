// ================= 头文件包含区 =================
// 引入自定义的倒计时门闩类：用于线程同步（如等待后端线程初始化完成）
#include "CountDownLatch.hpp"
// 引入自定义的日志文件类：封装了底层文件写入、滚动等逻辑（类似之前的 AppendFile 封装）
#include "LogFile.hpp"

// 引入 C++ 标准库
#include <string>       // std::string 用于存储日志消息和文件名
#include <atomic>       // std::atomic 原子变量，用于无锁的线程安全状态标记
#include <thread>       // std::thread 用于创建后端线程
#include <mutex>        // std::mutex 互斥锁，保护共享数据
#include <condition_variable> // std::condition_variable 条件变量，用于线程间通知
#include <memory>       // std::unique_ptr 智能指针，管理线程对象
#include <vector>       // std::vector 容器，用于存储已满的缓冲区

using namespace std;

// ================= 头文件保护 =================
#ifndef ASYNLOGGING_HPP
#define ASYNLOGGING_HPP

namespace logfile
{
    // ================= 异步日志类定义 =================
    // 核心思想：
    // 1. 前端线程（业务线程）调用 append()，只把日志写到内存缓冲区，不阻塞在磁盘 IO 上
    // 2. 后端线程（专门的 IO 线程）负责把缓冲区的数据批量写入磁盘
    // 3. 通过“双缓冲”或“多缓冲”机制，减少锁的争用
    class AsynLogging
    {
    private:
        // ================= 私有成员函数 =================
        // 后端工作线程的主函数
        // 功能：循环等待，条件满足时将缓冲区数据写入磁盘
        void workThreadFunc();

    private:
        // ================= 私有成员变量 (配置与状态) =================
        
        // 刷新间隔（单位：秒），例如 3 秒
        // 含义：即使缓冲区没写满，每隔 flushInterval_ 秒也要强制刷一次盘，防止日志长时间滞留在内存
        const int flushInterval_; 

        // 运行状态标记：原子布尔变量
        // std::atomic<bool>：保证对这个变量的读写是原子的（线程安全），不需要加锁
        // 用于告诉后端线程是否该退出循环
        std::atomic<bool> running_;

        // 日志文件的基础名称（不含路径和时间戳后缀）
        const std::string basename_;

        // 日志文件滚动大小（单位：字节），例如 128KB
        // 含义：当单个日志文件大小超过 rollSize_ 时，关闭旧文件，创建新文件（日志滚动）
        const size_t rollSize_;

        // ================= 私有成员变量 (线程与同步) =================

        // 指向后端工作线程的智能指针
        // std::unique_ptr：自动管理线程对象的生命周期，析构时自动处理
        std::unique_ptr<std::thread> pthread_;

        // 互斥锁：保护以下共享数据
        // 被保护的数据：currentBuffer_、buffers_
        std::mutex mutex_;

        // 条件变量：用于前端线程和后端线程的通信
        // 典型场景：
        // 1. 前端写满缓冲区 -> 通知后端来取
        // 2. 后端没活干 -> 阻塞等待，直到被通知或超时
        std::condition_variable cond_;

        // 倒计时门闩（CountDownLatch）：用于线程启动同步
        // 作用：主线程调用 start() 后，会阻塞在 latch_ 上，直到后端线程完全启动并初始化好，才继续往下走
        logfile::CountDownLatch latch_;

        // ================= 私有成员变量 (缓冲区) =================

        // 当前缓冲区（Current Buffer）
        // 前端线程（业务线程）主要往这个 buffer 里写日志
        // 注意：这里用 std::string 作为缓冲区，实际工程中常用固定大小的 char 数组（如 4MB/8MB）以避免频繁分配
        std::string currentBuffer_;

        // 已满缓冲区队列（Buffer Vector）
        // 当 currentBuffer_ 写满后，会被移动（move）到这个 vector 中，等待后端线程取走
        // 这是一个“生产者-消费者”队列：前端生产满 buffer，后端消费满 buffer
        std::vector<std::string> buffers_;

        // ================= 私有成员变量 (文件写入) =================

        // 实际的日志文件写入器
        // 封装了 fopen/fwrite/fflush 等操作，以及日志滚动逻辑
        logfile::LogFile output_;

    public:
        // ================= 公有接口 =================

        // 构造函数
        // 参数 bname: 日志文件基础名
        // 参数 rollSize: 文件滚动大小（默认 128KB）
        // 参数 flushInterval: 强制刷新间隔（默认 3秒）
        AsynLogging(const std::string &bname,
                    const size_t rollSize = 1024 * 128,
                    int flushInterval = 3);

        // 析构函数
        // 会自动调用 stop()，确保资源释放
        ~AsynLogging();

        // C++ 风格接口：追加日志
        // 前端线程（业务线程）调用此函数写日志
        void append(const std::string &msg);

        // C 风格接口：追加日志
        // 参数 msg: 字符指针；len: 长度
        void append(const char *msg, const size_t len);

        // 启动日志系统
        // 功能：创建后端工作线程，开始异步写入
        void start();

        // 停止日志系统
        // 功能：设置 running_ 为 false，通知后端线程退出，并等待线程结束
        void stop();

        // 强制刷新
        // 功能：通知后端线程立即将当前缓冲区数据写入磁盘
        void flush();
    };

} // namespace logfile

#endif // 结束头文件保护