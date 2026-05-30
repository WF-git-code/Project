// ================= 头文件包含 =================
// 引入日志消息类：负责实际存储和格式化日志内容（如时间戳、文本拼接）
#include "LogMessage.hpp"

// 引入 C++ 标准库
#include <functional> // std::function：用于定义可调用对象的类型（回调函数）
#include <string>     // std::string：用于存储文件名、函数名等
using namespace std;

// ================= 头文件保护 =================
#ifndef LOGGER_HPP
#define LOGGER_HPP

namespace logfile
{
    // ================= 日志器类定义 =================
    // 核心作用：
    // 1. 提供用户友好的接口（如 LOG_INFO << "hello"）
    // 2. 自动收集日志上下文（文件名、行号、函数名、时间戳）
    // 3. 通过全局回调函数将日志传递给后端（如 LogFile 或 AsynLogging）
    class Logger
    {
    public:
        // ================= 回调函数类型定义 =================
        // OutputFunc：日志输出回调类型
        // 定义：接收一个 const std::string&（格式化好的完整日志行），无返回值
        // 作用：用户可以通过 setOutput 设置自定义输出目标（如文件、网络、控制台）
        using OutputFunc = std::function<void(const std::string &)>;
        
        // FlushFunc：日志刷新回调类型
        // 定义：无参数，无返回值
        // 作用：用户可以通过 setFlush 设置自定义刷新逻辑（如强制刷盘）
        using FlushFunc = std::function<void(void)>;

    public:
        // ================= 静态回调函数成员 =================
        // 静态成员：全局共享的输出和刷新函数
        // 注意：静态成员变量需要在 .cpp 文件中定义（仅声明是不够的）
        static OutputFunc s_output_;
        static FlushFunc s_flush_;

        // ================= 静态回调设置函数 =================
        // 设置全局输出回调
        // 参数 func：用户自定义的输出函数
        static void setOutput(OutputFunc func);
        // 设置全局刷新回调
        // 参数 func：用户自定义的刷新函数
        static void setFlush(FlushFunc func);

    private:
        // ================= 日志消息实现成员 =================
        // 实际存储和处理日志内容的对象
        // 职责：拼接时间戳、日志级别、源文件信息、用户日志文本
        logfile::LogMessage impl_;

    public:
        // ================= 构造函数 =================
        // 功能：创建一个临时的 Logger 对象，收集日志上下文
        // 参数 level：日志级别（如 TRACE/INFO/ERROR）
        // 参数 filename：源文件名（由宏 __FILE__ 自动传入）
        // 参数 funcname：函数名（由宏 __func__ 自动传入）
        // 参数 line：行号（由宏 __LINE__ 自动传入）
        Logger(const logfile::LOG_LEVEL &level,
               const std::string &filename,
               const std::string &funcname,
               const int line);

        // ================= 析构函数 =================
        // 关键设计：日志输出的触发点！
        // 当临时 Logger 对象销毁时（通常是语句结束时），析构函数会自动调用
        // 析构函数内部会：
        //   1. 从 impl_ 获取完整的格式化日志字符串
        //   2. 调用全局的 s_output_ 回调输出日志
        //   3. 如果是 ERROR/FATAL 级别，可能还会调用 s_flush_ 强制刷盘
        ~Logger();

        // ================= 流式接口 =================
        // 功能：返回 impl_ 的引用，支持流式写入语法（如 LOG_INFO << "hello" << 123）
        // 返回值：LogMessage&，该类通常重载了 operator<< 来拼接各种类型的数据
        logfile::LogMessage &stream();

    private:
        // ================= 全局日志级别 =================
        // 静态成员：全局的日志输出级别
        // 作用：只有当日志消息的级别 >= s_level_ 时，才会被实际输出
        static logfile::LOG_LEVEL s_level_;

    public:
        // ================= 日志级别访问函数 =================
        // 获取当前全局日志级别
        static logfile::LOG_LEVEL getLogLevel();
        // 设置全局日志级别
        // 参数 level：新的日志级别
        static void setLogLevel(const logfile::LOG_LEVEL &level);
    };

    // ================= 日志宏定义（用户接口核心） =================
    // 设计技巧：使用宏自动注入上下文信息（__FILE__, __func__, __LINE__）
    // 语法：LOG_INFO << "这是一条日志" << 变量;

    // TRACE 级别日志
    // 逻辑：先检查当前全局级别是否 <= TRACE，若是则创建临时 Logger 对象并返回流
    #define LOG_TRACE                                                \
        if (logfile::Logger::getLogLevel() <= logfile::LOG_LEVEL::TRACE) \
        logfile::Logger(logfile::LOG_LEVEL::TRACE, __FILE__, __func__, __LINE__).stream()

    // DEBUG 级别日志
    #define LOG_DEBUG                                                \
        if (logfile::Logger::getLogLevel() <= logfile::LOG_LEVEL::DEBUG) \
        logfile::Logger(logfile::LOG_LEVEL::DEBUG, __FILE__, __func__, __LINE__).stream()

    // INFO 级别日志
    #define LOG_INFO                                                \
        if (logfile::Logger::getLogLevel() <= logfile::LOG_LEVEL::INFO) \
        logfile::Logger(logfile::LOG_LEVEL::INFO, __FILE__, __func__, __LINE__).stream()

    // WARN 级别日志
    #define LOG_WARN                                                \
        if (logfile::Logger::getLogLevel() <= logfile::LOG_LEVEL::WARN) \
        logfile::Logger(logfile::LOG_LEVEL::WARN, __FILE__, __func__, __LINE__).stream()

    // ERROR 级别日志（无条件输出，不做级别检查）
    #define LOG_ERROR \
        logfile::Logger(logfile::LOG_LEVEL::ERROR, __FILE__, __func__, __LINE__).stream()

    // FATAL 级别日志（无条件输出，通常会导致程序终止）
    #define LOG_FATAL \
        logfile::Logger(logfile::LOG_LEVEL::FATAL, __FILE__, __func__, __LINE__).stream()

    // SYSERR/SYSFATAL：通常用于系统调用错误（可扩展为包含 strerror(errno)）
    #define LOG_SYSERR \
        logfile::Logger(logfile::LOG_LEVEL::ERROR, __FILE__, __func__, __LINE__).stream()

    #define LOG_SYSFATAL \
        logfile::Logger(logfile::LOG_LEVEL::FATAL, __FILE__, __func__, __LINE__).stream()

}

#endif // 结束头文件保护