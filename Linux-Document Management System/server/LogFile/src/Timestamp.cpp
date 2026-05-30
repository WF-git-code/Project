#include "Timestamp.hpp"
#include "LogCommon.hpp"
#include <time.h>      // 引入 C 时间库：用于 localtime_r、tm 结构体
#include <sys/time.h>  // 引入 POSIX 时间库：用于 gettimeofday、timeval 结构体
#include <stdio.h>     // 引入 C 标准 I/O：用于 sprintf 字符串格式化
namespace logfile
{
    // ================= 构造函数实现 =================
    // 功能：用微秒数初始化时间戳对象
    // 参数 ms：自 Unix 纪元以来的微秒数，默认值为 0
    Timestamp::Timestamp(uint64_t ms) : micro_(ms) {}

    // ================= 析构函数实现 =================
    // 无特殊操作，使用编译器默认生成的析构逻辑
    Timestamp::~Timestamp() {}

    // ================= 交换函数实现 =================
    // 功能：高效交换两个 Timestamp 对象的内部微秒值
    // 参数 te：要交换的另一个 Timestamp 对象
    void Timestamp::swap(Timestamp &te)
    {
        // 调用标准库的 swap 函数，仅交换内部成员变量 micro_
        std::swap(this->micro_, te.micro_);
    }

    // ================= 转微秒字符串实现 =================
    // 功能：将时间戳转换为 "秒数.微秒数" 的原始字符串格式
    // 返回值：例如 "1708594200.123456"
    std::string Timestamp::to_string() const
    {
        // 使用 LogCommon.hpp 中定义的小缓冲区（128字节）
        char buff[SMALL_BUFF_SIZE] = {};
        
        // 计算秒数：微秒数 / 每秒微秒数（1000*1000）
        time_t ss = micro_ / KMicS;
        // 计算剩余微秒数：微秒数 % 每秒微秒数
        time_t ms = micro_ % KMicS;
        
        // 格式化字符串："%lu.%lu" 对应无符号长整数
        sprintf(buff, "%lu.%lu", ss, ms);
        return buff;
    }

    // ================= 日志显示格式化字符串实现 =================
    // 功能：将时间戳转换为人类可读的日志格式
    // 参数 showms：是否显示微秒部分，默认 true
    // 返回值：例如 "2026/02/22-14:30:25.123456Z"
    std::string Timestamp::toFormattedString(bool showms) const
    {
        char buff[SMALL_BUFF_SIZE] = {};
        
        // 拆分秒数和剩余微秒数
        time_t ss = micro_ / KMicS;
        time_t ms = micro_ % KMicS;
        
        // 定义 tm 结构体：用于存储分解后的时间（年、月、日、时、分、秒）
        struct tm dtm = {};
        
        // localtime_r：线程安全的时间转换函数
        // 作用：将秒级时间戳（ss）转换为本地时间的 tm 结构体
        // 注意：localtime 不是线程安全的，必须用 localtime_r（_r 表示 reentrant，可重入）
        localtime_r(&ss, &dtm);
        
        // 第一部分格式化：年/月/日-时:分:秒
        // 关键调整：
        //   - dtm.tm_year：从 1900 年开始计数，所以要 +1900
        //   - dtm.tm_mon：从 0 开始计数（0=1月），所以要 +1
        int pos = sprintf(buff, "%4d/%02d/%02d-%02d:%02d:%02d",
                          dtm.tm_year + 1900,
                          dtm.tm_mon + 1,
                          dtm.tm_mday,
                          dtm.tm_hour,
                          dtm.tm_min,
                          dtm.tm_sec);
        
        // 第二部分格式化：如果 showms 为 true，追加微秒部分
        if (showms)
        {
            // buff + pos：从第一部分结束的位置继续写入
            sprintf(buff + pos, ".%ldZ", ms);
        }
        return buff;
    }

    // ================= 文件名格式化字符串实现 =================
    // 功能：将时间戳转换为适合文件名的格式（避免特殊字符）
    // 返回值：例如 "2026-02-22_143025.123456Z"
    std::string Timestamp::toFormattedFile() const
    {
        char buff[SMALL_BUFF_SIZE] = {};
        
        // 拆分秒数和剩余微秒数
        time_t ss = micro_ / KMicS;
        time_t ms = micro_ % KMicS;
        
        struct tm dtm = {};
        // 线程安全的本地时间转换
        localtime_r(&ss, &dtm);
        
        // 格式化：年-月-日_时分秒（用下划线和连字符，避免冒号等文件名非法字符）
        int pos = sprintf(buff, "%4d-%02d-%02d_%02d%02d%02d",
                          dtm.tm_year + 1900,
                          dtm.tm_mon + 1,
                          dtm.tm_mday,
                          dtm.tm_hour,
                          dtm.tm_min,
                          dtm.tm_sec);

        // 追加微秒部分
        sprintf(buff + pos, ".%ldZ", ms);
        return buff;
    }

    // ================= 判断时间戳合法性实现 =================
    // 功能：检查 micro_ 是否大于 0（0 表示无效时间）
    // 返回值：true（合法）；false（非法）
    bool Timestamp::valid() const { return micro_ > 0; }

    // ================= 获取秒级时间戳实现 =================
    // 返回值：自 Unix 纪元以来的秒数
    time_t Timestamp::getSecond() const { return micro_ / KMicS; }

    // ================= 获取微秒级时间戳实现 =================
    // 返回值：自 Unix 纪元以来的原始微秒数
    uint64_t Timestamp::getMicro() const { return micro_; }

    // ================= 获取毫秒级时间戳实现 =================
    // 返回值：自 Unix 纪元以来的毫秒数
    uint64_t Timestamp::getMills() const { return micro_ / KMilS; }

    // ================= 成员函数：设置为当前时间实现 =================
    // 功能：将当前对象更新为当前系统时间
    // 返回值：当前对象的引用（支持链式调用）
    const Timestamp &Timestamp::now()
    {
        // 调用静态函数 Now() 创建当前时间对象，赋值给自己
        *this = Timestamp::Now();
        return *this;
    }

    // ================= 类型转换运算符实现 =================
    // 功能：将 Timestamp 对象隐式转换为 uint64_t（微秒数）
    // 作用：方便直接比较（如 t1 < t2）和计算
    Timestamp::operator uint64_t() const
    {
        return micro_;
    }

    // ================= 静态函数：获取当前时间实现 =================
    // 功能：创建一个新的 Timestamp 对象，初始化为当前系统时间
    // 返回值：当前时间的 Timestamp 对象
    Timestamp Timestamp::Now()
    {
        // 定义 timeval 结构体：包含 tv_sec（秒）和 tv_usec（微秒）
        struct timeval tv;
        
        // gettimeofday：POSIX 系统调用，获取当前高精度时间
        // 参数 1：timeval 结构体指针（输出参数）
        // 参数 2：时区指针（通常设为 nullptr，使用本地时区）
        gettimeofday(&tv, nullptr);
        
        // 提取秒数
        uint64_t seconds = tv.tv_sec;
        // 组装成总微秒数：秒数 * 1000000 + 微秒数
        return Timestamp(seconds * KMicS + tv.tv_usec);
    }

    // ================= 静态函数：获取无效时间戳实现 =================
    // 功能：创建一个 micro_ 为 0 的无效时间戳对象
    // 返回值：无效的 Timestamp 对象
    Timestamp Timestamp::Invalid()
    {
        return Timestamp(0);
    }

} // namespace logfile