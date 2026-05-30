#include "Timestamp.hpp"
#include "LogMessage.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
using namespace std;

namespace logfile
{
    // ================= 日志级别转字符串数组定义 =================
    const char *LLtoStr[] = {
        "TRACE",     // 对应 LOG_LEVEL::TRACE
        "DEBUG",     // 对应 LOG_LEVEL::DEBUG
        "INFO",      // 对应 LOG_LEVEL::INFO
        "WARN",      // 对应 LOG_LEVEL::WARN
        "ERROR",     // 对应 LOG_LEVEL::ERROR
        "FATAL",     // 对应 LOG_LEVEL::FATAL
        "NUM_LOG_LEVEL" // 对应 NUM_LOG_LEVEL
    };

    // ================= 构造函数实现（核心：生成日志头部） =================
    // 功能：收集并格式化所有日志元信息，生成 header_
    // 参数 level：日志级别；参数 filename：源文件完整路径；
    // 参数 funcname：函数名；参数 line：行号
    LogMessage::LogMessage(const logfile::LOG_LEVEL &level,
                           const std::string &filename,
                           const std::string &funcname,
                           const int line)
        : header_{}, text_{}, level_(level) // 初始化列表：清空头部和文本，保存日志级别
    {
        std::stringstream ss; // 使用 stringstream 进行字符串拼接（比 + 号更高效、更方便）

        // 1. 拼接时间戳
        // 调用 Timestamp::Now() 获取当前时间，再调用 toFormattedString() 格式化为可读字符串
        ss << logfile::Timestamp::Now().toFormattedString() << " ";

        // 2. 拼接线程 ID
        // std::this_thread::get_id()：C++11 标准库函数，获取当前线程的唯一标识符
        // 作用：在多线程环境下，方便追踪是哪个线程输出的日志
        ss << std::this_thread::get_id() << " ";

        // 3. 拼接日志级别字符串
        // static_cast<int>(level_)：将 enum class 强转为 int，用于数组索引
        // LLtoStr[]：在 LogCommon.hpp 中定义的字符串数组，将枚举值转为 "INFO"/"ERROR" 等字符串
        ss << logfile::LLtoStr[static_cast<int>(level_)] << " ";

        // 4. 提取文件名（去掉路径）
        // filename 通常是完整路径（如 "/home/user/project/main.cpp"）
        // find_last_of('/')：找到最后一个 '/' 的位置
        const size_t pos = filename.find_last_of('/');
        // substr(pos + 1)：截取最后一个 '/' 之后的部分，得到纯文件名（如 "main.cpp"）
        std::string fname = filename.substr(pos + 1);

        // 5. 拼接文件名、函数名、行号
        ss << fname << " " << funcname << " " << line << " ";

        // 6. 将 stringstream 中的所有内容转为 string，赋值给 header_
        header_ = ss.str();
    }

    // ================= 析构函数实现 =================
    // 此处无特殊操作，使用编译器默认生成的析构逻辑即可
    LogMessage::~LogMessage()
    {
    }

    // ================= 获取日志级别 =================
    // 功能：返回当前日志消息的级别
    // const 修饰：保证不修改对象状态
    const logfile::LOG_LEVEL &LogMessage::getLogLevel() const
    {
        return level_;
    }

    // ================= 拼接完整日志字符串 =================
    // 功能：将日志头部（header_）和用户文本（text_）拼接起来
    // 返回值：完整的一行日志字符串
    const std::string LogMessage::toString() const
    {
        return header_ + text_;
    }

} // namespace logfile