#include "LogCommon.hpp"
#include "LogFile.hpp"
// 引入 POSIX 标准库：用于 gethostname()（获取主机名）和 getpid()（获取进程 ID）
#include <unistd.h>
#include <string>

namespace logfile {

// ================= 辅助静态函数：获取主机名 =================
// 作用：生成日志文件名时，将主机名嵌入，方便在多机器环境下区分日志来源
static std::string hostname() {
    // 定义缓冲区，大小使用 LogCommon.hpp 中定义的 SMALL_BUFF_SIZE (128)
    char buff[SMALL_BUFF_SIZE] = {};
    // ::gethostname：POSIX 系统调用，获取当前主机名
    // 参数 1：缓冲区指针；参数 2：缓冲区大小
    if (::gethostname(buff, SMALL_BUFF_SIZE) == 0) {
        return std::string(buff); // 成功，返回主机名
    } else {
        return std::string("unknownhost"); // 失败，返回默认值
    }
}

// ================= 辅助静态函数：获取进程 ID =================
// 作用：生成日志文件名时，将进程 ID 嵌入，方便区分同一机器上的多个进程实例
static pid_t pid() {
    // ::getpid：POSIX 系统调用，获取当前进程的 ID
    return ::getpid();
}

// ================= 静态成员函数：生成带时间戳的日志文件名 =================
// 命名规则：basename_YYYYMMDD-HHMMSS.hostname.pid.log
// 示例：mylog_20260222-143025.myhost.12345.log
std::string LogFile::getLogFileName(const std::string &basename, const logfile::Timestamp &now) {
    std::string logfilename;
    // 预分配空间：避免频繁的内存重新分配（性能优化）
    // 大小 = basename 长度 + 小缓冲区（足够容纳时间、主机名、pid 等）
    logfilename.reserve(basename.size() + SMALL_BUFF_SIZE);
    
    // 拼接文件名各部分
    logfilename = basename;                  // 1. 基础名
    logfilename += ".";
    logfilename += now.toFormattedFile();   // 2. 时间戳（Timestamp 类的方法，生成适合文件名的格式）
    logfilename += ".";
    logfilename += hostname();               // 3. 主机名
    logfilename += ".";
    logfilename += std::to_string(pid());   // 4. 进程 ID
    logfilename += ".log";                   // 5. 后缀
    
    printf("【调试10】生成的日志文件名：%s\n", logfilename.c_str());
    return logfilename;
}

// ================= 私有成员函数：不加锁的追加逻辑（核心） =================
// 作用：实际的写入、滚动检查、刷新检查逻辑，由 public 接口加锁后调用
void LogFile::append_unlocked(const char *msg, const size_t len) {
    // 1. 先调用底层 AppendFile 写入数据
    file_->append(msg, len);

    // 2. 检查是否需要按大小滚动日志
    // 如果当前文件已写字节数 > 设定的滚动大小（rollSize_），立即滚动
    if (file_->getWriteBytes() > rollSize_) {
        rollFile();
    } else {
        // 3. 未达到滚动大小，进行“周期性检查”
        count_ += 1; // 计数器 +1
        
        // 只有当计数器达到 checkEventN_（如 30 次）时，才做一次耗时的时间检查（性能优化）
        if (count_ > checkEventN_) {
            count_ = 0; // 重置计数器
            time_t now = ::time(nullptr); // 获取当前 Unix 时间戳（秒）
            
            // 计算当前周期的起始时间（即当天的 00:00:00）
            // 技巧：整数除法舍去余数，再乘回来，得到当天 0 点的时间戳
            time_t thisPeriod = (now / kPollPerSeconds_) * kPollPerSeconds_;

            // 4. 检查是否需要按天滚动日志
            // 如果当前周期 > 上次记录的周期，说明跨天了，需要滚动
            if (thisPeriod > startOfPeriod_) {
                rollFile();
            } 
            // 5. 检查是否需要强制刷新
            // 如果当前时间 - 上次刷新时间 > 设定的刷新间隔（flushInterval_），强制刷盘
            else if (now - lastFlush_ > flushInterval_) {
                lastFlush_ = now;
                file_->flush();
            }
        }
    }
}

// ================= 构造函数实现 =================
LogFile::LogFile(const std::string &bname,
                 size_t rollsize,
                 int flushInterval,
                 int checkEventN,
                 bool threadSafe)
    // 初始化列表（按成员声明顺序初始化）
    : basename_(bname),
      rollSize_(rollsize),
      flushInterval_(flushInterval),
      checkEventN_(checkEventN),
      count_(0),
      startOfPeriod_(0),
      lastRoll_(0),
      lastFlush_(0),
      file_(nullptr),
      // 关键：根据 threadSafe 参数决定是否创建互斥锁
      // 如果 threadSafe=true，new 一个 mutex；否则 mutex_ 为 nullptr（不使用锁，性能更高）
      mutex_(threadSafe ? new std::mutex{} : nullptr) {
    // 构造函数最后调用 rollFile()
    // 作用：创建第一个日志文件，初始化 file_ 对象
    rollFile();
}

// ================= 析构函数实现 =================
// 使用 =default：让编译器自动生成析构函数
// 效果：unique_ptr<file_> 和 unique_ptr<mutex_> 会自动调用析构函数，释放资源
LogFile::~LogFile() = default;

// ================= C++ 风格 append 接口 =================
void LogFile::append(const std::string &msg) {
    // 转发给 C 风格接口
    append(msg.c_str(), msg.size());
}

// ================= C 风格 append 接口（线程安全入口） =================
void LogFile::append(const char *msg, const size_t len) {
    // 检查是否需要加锁
    if (mutex_) {
        // 需要线程安全：加锁，然后调用不加锁的核心逻辑
        std::unique_lock<std::mutex> locker(*mutex_);
        append_unlocked(msg, len);
    } else {
        // 不需要线程安全：直接调用核心逻辑（性能最优）
        append_unlocked(msg, len);
    }
}

// ================= flush 接口（线程安全） =================
void LogFile::flush() {
    // 同样的线程安全判断逻辑
    if (mutex_) {
        std::unique_lock<std::mutex> locker(*mutex_);
        file_->flush();
    } else {
        file_->flush();
    }
}

// ================= 日志滚动实现 =================
// 功能：关闭当前文件，生成新文件名，创建新文件
bool LogFile::rollFile() {
    // 1. 获取当前时间
    logfile::Timestamp now;
    now.now(); // 假设 Timestamp::now() 方法用于获取当前时间
    
    // 2. 生成新的日志文件名
    std::string filename = getLogFileName(basename_, now);
    
    // 3. 计算新的周期起始时间（当天 00:00:00）
    time_t start = (now.getSecond() / kPollPerSeconds_) * kPollPerSeconds_;
    
    // 4. 防抖动检查：确保同一秒内不会多次滚动
    // 只有当前时间 > 上次滚动时间，才执行滚动
    if (now.getSecond() > lastRoll_) {
        // 5. 更新时间记录
        lastRoll_ = now.getSecond();       // 更新上次滚动时间
        lastFlush_ = now.getSecond();      // 更新上次刷新时间
        startOfPeriod_ = start;            // 更新周期起始时间
        
        // 6. 创建新的 AppendFile 对象，替换旧的
        // unique_ptr::reset() 会先释放旧的 file_（关闭旧文件），再接管新的对象
        file_.reset(new logfile::AppendFile(filename));
        
        printf("【调试11】日志文件轮转成功，新文件：%s\n", filename.c_str());
        return true;
    }
    return false; // 同一秒内，未滚动
}

} // namespace logfile