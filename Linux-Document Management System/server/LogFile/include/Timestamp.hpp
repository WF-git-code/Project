// ================= 头文件包含 =================
#include <cstdint> // 引入标准整数类型：uint64_t（64位无符号整数）
#include <string>  // 引入字符串类：用于时间格式化输出
#ifndef TIMESTAMP_HPP
#define TIMESTAMP_HPP
namespace logfile
{
    // ================= 时间戳类定义 =================
    // 核心作用：
    // 1. 存储高精度时间（微秒级）
    // 2. 提供当前时间获取功能
    // 3. 支持多种时间格式转换（用于日志显示、文件名生成）
    // 4. 提供时间计算功能（差值、加法）
    class Timestamp
    {
    private:
        // ================= 私有成员变量 =================
        // 存储自 Unix 纪元（1970-01-01 00:00:00 UTC）以来的微秒数
        // 使用 uint64_t：64位无符号整数，可存储约58万年的微秒数，足够使用
        uint64_t micro_; 

    public:
        // ================= 构造函数 =================
        // 功能：用微秒数初始化时间戳
        // 参数 ms：微秒数，默认值为 0（表示无效时间戳）
        explicit Timestamp(uint64_t ms = 0);

        // ================= 析构函数 =================
        ~Timestamp();

        // ================= 交换函数 =================
        // 功能：交换两个 Timestamp 对象的内部微秒值
        // 参数 te：要交换的另一个 Timestamp 对象
        // 作用：比赋值操作更高效（仅交换内部成员变量）
        void swap(Timestamp &te);

        // ================= 转微秒字符串 =================
        // 功能：将时间戳转换为微秒数的字符串表示
        // 返回值：例如 "1708594200000000"
        std::string to_string() const;

        // ================= 格式化字符串（用于日志显示） =================
        // 功能：将时间戳转换为人类可读的格式化字符串
        // 参数 showms：是否显示微秒部分，默认 true
        // 返回值：
        //   - showms=true: "20260222.143025.123456"（年月日.时分秒.微秒）
        //   - showms=false: "20260222.143025"（仅年月日.时分秒）
        std::string toFormattedString(bool showms = true) const;

        // ================= 格式化字符串（用于文件名） =================
        // 功能：将时间戳转换为适合文件名的格式（避免使用特殊字符）
        // 返回值：例如 "20260222_143025.123456"（用下划线分隔日期和时间）
        // 注意：文件名中不能包含冒号等特殊字符，因此用下划线替代
        std::string toFormattedFile() const;

        // ================= 判断时间戳是否合法 =================
        // 功能：检查时间戳是否为有效时间
        // 返回值：true（合法，micro_ > 0）；false（非法，micro_ = 0）
        bool valid() const;

        // ================= 获取秒级时间戳 =================
        // 功能：返回自 Unix 纪元以来的秒数
        // 返回值：time_t（通常是 32 位或 64 位整数）
        time_t getSecond() const;

        // ================= 获取微秒级时间戳 =================
        // 功能：返回自 Unix 纪元以来的微秒数（原始值）
        // 返回值：uint64_t（64位无符号整数）
        uint64_t getMicro() const;

        // ================= 获取毫秒级时间戳 =================
        // 功能：返回自 Unix 纪元以来的毫秒数
        // 返回值：uint64_t
        uint64_t getMills() const;

        // ================= 成员函数：设置为当前时间 =================
        // 功能：将当前对象的 micro_ 更新为当前系统时间的微秒数
        // 返回值：const Timestamp&，返回当前对象的引用（支持链式调用）
        const Timestamp &now();

        // ================= 类型转换运算符 =================
        // 功能：将 Timestamp 对象隐式转换为 uint64_t（微秒数）
        // 作用：方便 Timestamp 对象直接参与比较（如 a < b）和计算
        operator uint64_t() const;

    public:
        // ================= 静态成员函数：获取当前时间 =================
        // 功能：创建一个新的 Timestamp 对象，初始化为当前系统时间
        // 返回值：Timestamp，当前时间的时间戳对象
        static Timestamp Now();

        // ================= 静态成员函数：获取无效时间戳 =================
        // 功能：创建一个无效的 Timestamp 对象（micro_ = 0）
        // 返回值：Timestamp，无效时间戳
        static Timestamp Invalid();

        // ================= 静态常量：时间单位换算 =================
        // 每秒的微秒数（1秒 = 1000 * 1000 微秒）
        static const int KMicS = 1000 * 1000;
        // 每毫秒的微秒数（1毫秒 = 1000 微秒）
        static const int KMilS = 1000;
        // 补充说明：1秒 = 1000毫秒 = 1000*1000微秒 = 1000*1000*1000纳秒
    };

    // ================= 内联函数：计算时间差（秒） =================
    // 参数 a：较晚的时间；参数 b：较早的时间
    // 返回值：a - b 的秒数
    inline time_t diffSecond(const Timestamp &a, const Timestamp &b)
    {
        return a.getSecond() - b.getSecond();
    }

    // ================= 内联函数：计算时间差（毫秒） =================
    // 返回值：a - b 的毫秒数
    inline uint64_t diffMills(const Timestamp &a, const Timestamp &b)
    {
        return a.getMills() - b.getMills();
    }

    // ================= 内联函数：计算时间差（微秒） =================
    // 返回值：a - b 的微秒数
    inline uint64_t diffMicro(const Timestamp &a, const Timestamp &b)
    {
        return a.getMicro() - b.getMicro();
    }

    // ================= 内联函数：时间加法（秒） =================
    // 参数 a：基础时间；参数 sec：要增加的秒数
    // 返回值：新的 Timestamp 对象（a + sec 秒）
    inline Timestamp addTimeSecond(const Timestamp &a, time_t sec)
    {
        return Timestamp(a.getMicro() + sec * logfile::Timestamp::KMicS);
    }

    // ================= 内联函数：时间加法（毫秒） =================
    // 参数 mills：要增加的毫秒数
    inline Timestamp addTimeMills(const Timestamp &a, uint64_t mills)
    {
        return Timestamp(a.getMicro() + mills * logfile::Timestamp::KMilS);
    }

    // ================= 内联函数：时间加法（微秒） =================
    // 参数 micro：要增加的微秒数
    inline Timestamp addTimeMicors(const Timestamp &a, uint64_t micro)
    {
        return Timestamp(a.getMicro() + micro);
    }

}

#endif // 结束头文件保护