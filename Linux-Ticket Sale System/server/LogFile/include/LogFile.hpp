// ================= 自定义头文件包含 =================
// 引入时间戳类：用于生成带时间戳的日志文件名、记录滚动/刷新时间
#include "Timestamp.hpp"
// 引入底层文件追加类：封装了 fwrite/fflush 等核心 IO 操作
#include "AppendFile.hpp"

// ================= C 标准库包含 =================
// 引入 C 时间库：用于 time_t 类型、时间计算（如 time() 函数）
#include <time.h>

// ================= C++ 标准库包含 =================
#include <string>       // std::string 用于存储文件名和日志消息
#include <memory>       // std::unique_ptr 智能指针，管理 AppendFile 和 mutex
#include <mutex>        // std::mutex 互斥锁，用于线程安全
using namespace std;

// ================= 头文件保护 =================
#ifndef LOGFILE_HPP
#define LOGFILE_HPP

namespace logfile
{
    // ================= 日志文件管理类定义 =================
    // 核心功能：
    // 1. 封装 AppendFile，提供更高级的文件写入
    // 2. 日志滚动（Rolling）：文件大小超过 rollSize_ 或跨天时，自动创建新文件
    // 3. 自动刷新：每隔 flushInterval_ 秒或每写 checkEventN_ 次，强制刷盘
    // 4. 可选线程安全：通过 threadSafe 参数控制是否加锁
    class LogFile
    {
    private:
        // ================= 配置相关成员变量 =================
        
        // 日志文件的基础名称（不含时间戳和后缀）
        const std::string basename_;
        
        // 日志文件滚动大小（单位：字节），例如 128KB
        // 含义：当单个文件写入字节数超过此值时，触发日志滚动（创建新文件）
        const size_t rollSize_;

        // 强制刷新间隔（单位：秒），例如 3 秒
        // 含义：距离上次刷新超过此时间，即使没写满，也强制刷盘
        const int flushInterval_;
        
        // 检查频率（每写多少次检查一次滚动/刷新），例如 30 次
        // 含义：每调用 append() checkEventN_ 次，才检查一次是否需要滚动或刷新
        // 作用：避免每次 append 都做时间检查，提高性能
        const int checkEventN_;
        
        // 计数器：记录当前已经调用 append() 的次数
        // 当 count_ >= checkEventN_ 时，触发检查并重置为 0
        int count_;

        // ================= 时间相关成员变量 =================
        
        // 当前周期的开始时间（通常是当天的 00:00:00）
        // 用于按天滚动日志（每天一个新文件）
        time_t startOfPeriod_;
        
        // 上次日志滚动的时间戳
        time_t lastRoll_;
        
        // 上次强制刷新的时间戳
        time_t lastFlush_;

        // 静态常量：一天的秒数（60秒 * 60分钟 * 24小时）
        // 用于判断是否跨天，是否需要按天滚动日志
        static const int kPollPerSeconds_ = 60 * 60 * 24;

        // ================= 资源管理成员变量 =================
        
        // 底层文件写入器的智能指针
        // 实际的 IO 操作（fwrite/fflush）都委托给这个 AppendFile 对象
        std::unique_ptr<logfile::AppendFile> file_;
        
        // 互斥锁的智能指针
        // 注意：是 unique_ptr，意味着可以根据 threadSafe 参数决定是否创建锁
        // 如果 threadSafe=false，mutex_ 为 nullptr，不进行加锁（性能更高）
        std::unique_ptr<std::mutex> mutex_;

    private:
        // ================= 私有成员函数 =================
        
        // 静态函数：生成带时间戳的日志文件名
        // 参数 basename：基础名；参数 now：当前时间戳
        // 返回值：格式类似 "basename_20260222-123456.log" 的完整文件名
        static std::string getLogFileName(const std::string &basename, const logfile::Timestamp &now);
        
        // 不加锁的追加函数
        // 核心逻辑：实际写入操作，由 public 的 append() 加锁后调用
        // 作用：分离“加锁逻辑”和“写入逻辑”，代码更清晰
        void append_unlocked(const char *msg, const size_t len);

    public:
        // ================= 公有接口 =================
        
        // 构造函数
        // 参数 bname：日志文件基础名
        // 参数 rollsize：文件滚动大小（默认 128KB）
        // 参数 flushInterval：强制刷新间隔（默认 3秒）
        // 参数 checkEventN：检查频率（默认每 30 次 append 检查一次）
        // 参数 threadSafe：是否线程安全（默认 true，会创建 mutex）
        LogFile(const std::string &bname,
                size_t rollsize = 1024 * 128,
                int flushInterval = 3,
                int checkEventN = 30,
                bool threadSafe = true);

        // 析构函数
        // 自动 flush 剩余数据，释放资源
        ~LogFile();

        // C++ 风格接口：追加日志
        // 内部会根据 threadSafe 决定是否加锁
        void append(const std::string &msg);

        // C 风格接口：追加日志
        // 参数 msg：字符指针；参数 len：长度
        void append(const char *msg, const size_t len);

        // 强制刷新磁盘
        // 内部会根据 threadSafe 决定是否加锁
        void flush();

        // 手动触发日志滚动
        // 功能：关闭当前文件，创建一个新的带时间戳的日志文件
        // 返回值：是否滚动成功
        bool rollFile();
    };

} // namespace logfile

#endif // 结束头文件保护