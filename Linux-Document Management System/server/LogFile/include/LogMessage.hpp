// ================= 头文件包含 =================
// 引入日志公共定义：包含 LOG_LEVEL 枚举类
#include "LogCommon.hpp"
// 引入时间戳类：用于生成日志的时间信息
#include "Timestamp.hpp"

// 引入 C++ 标准库
#include <string>       // std::string：存储日志头和日志文本
#include <sstream>      // std::stringstream：用于将任意类型转换为字符串
using namespace std;

// ================= 头文件保护 =================
#ifndef LOG_MESSAGE_HPP
#define LOG_MESSAGE_HPP

namespace logfile
{
    // ================= 日志消息类定义 =================
    // 核心作用：
    // 1. 封装日志的“头部信息”（时间戳、级别、文件名、行号、函数名）
    // 2. 存储用户通过流式语法输入的“日志文本”
    // 3. 提供 toString() 方法，将头部和文本拼接成完整的日志行
    class LogMessage
    {
    private:
        // ================= 私有成员变量 =================
        
        // 日志头部：存储元信息（时间戳、日志级别、文件名、行号、函数名）
        // 格式示例："2026-02-22 14:30:25 [INFO] main.cpp:123(main)"
        std::string header_;
        
        // 日志文本：存储用户通过 << 运算符输入的内容
        // 格式示例：" : 用户输入的日志内容 : 123 : 3.14"
        std::string text_;
        
        // 日志级别：记录当前日志的级别（如 TRACE/INFO/ERROR）
        // 用于析构函数中判断是否需要终止程序（FATAL 级别）
        logfile::LOG_LEVEL level_;

    public:
        // ================= 构造函数 =================
        // 功能：初始化日志消息，生成日志头部
        // 参数 level：日志级别
        // 参数 filename：源文件名（由宏 __FILE__ 传入）
        // 参数 funcname：函数名（由宏 __func__ 传入）
        // 参数 line：行号（由宏 __LINE__ 传入）
        LogMessage(const logfile::LOG_LEVEL &level,
                   const std::string &filename,
                   const std::string &funcname,
                   const int line);

        // ================= 析构函数 =================
        // 作用：清理资源（此处无特殊操作，主要由编译器自动生成）
        ~LogMessage();

        // ================= 获取日志级别 =================
        // 功能：返回当前日志的级别
        // const 修饰：只读函数，不修改成员变量
        // 返回值：const logfile::LOG_LEVEL&，日志级别的常量引用
        const logfile::LOG_LEVEL &getLogLevel() const;

        // ================= 拼接完整日志字符串 =================
        // 功能：将日志头部（header_）和日志文本（text_）拼接成完整的一行日志
        // const 修饰：只读函数
        // 返回值：std::string，完整的日志字符串
        const std::string toString() const;

        // ================= 模板重载 << 运算符（核心流式语法） =================
        // 函数模板：支持任意类型的输入（如 int、double、std::string、自定义类型等）
        // 参数 val：用户输入的任意类型数据
        // 返回值：LogMessage&，返回当前对象的引用，实现链式调用（如 LOG_INFO << a << b << c）
        template <class _Ty>
        LogMessage &operator<<(const _Ty &val)
        {
            // 1. 创建 stringstream 对象，用于类型转换
            std::stringstream ss;
            
            // 2. 将数据转换为字符串，并在前面添加 " : " 作为分隔符
            // 例如：val 是 123，则 ss 中内容为 " : 123"
            ss << " : " << val;
            
            // 3. 将转换后的字符串追加到日志文本（text_）中
            text_ += ss.str();
            
            // 4. 返回当前对象的引用，支持链式调用
            return *this;
        }
    };
} // namespace logfile

#endif // 结束头文件保护