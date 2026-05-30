// ================= 实现文件头文件包含 =================
// 引入 C 标准断言库：用于在调试模式下检查条件 (如 fp_ != nullptr)
#include <assert.h>
// 引入 C 字符串库：用于 strerror() (错误码转字符串)
#include <string.h>

// 引入自己的头文件：包含类的声明
#include "AppendFile.hpp"

namespace logfile
{
    // ================= 私有 write 函数实现 =================
    // 功能：封装底层的文件写入操作
    // 参数 msg：待写入数据的指针；len：待写入的字节数
    // 返回值：实际写入的字节数
    size_t AppendFile::write(const char *msg, const size_t len)
    {
        // ::fwrite_unlocked：C 标准库函数
        // 1. 前面的 :: 表示使用“全局作用域”的函数，避免与类内可能的同名函数冲突
        // 2. fwrite_unlocked：非线程安全版本的 fwrite
        //    - 为什么不用 fwrite？因为 fwrite 内部会加锁 (mutex) 保证线程安全
        //    - 这里假设上层调用者（如日志系统）会自己加锁，所以用 _unlocked 版本提高性能
        // 3. 参数解释：
        //    - msg: 数据指针
        //    - sizeof(char): 每个元素的大小 (char 固定为 1 字节)
        //    - len: 元素的个数
        //    - fp_: 文件指针
        return ::fwrite_unlocked(msg, sizeof(char), len, fp_);
    }

    // ================= 构造函数实现 =================
    // 功能：初始化对象、分配缓冲区、打开文件
    // 参数 filename：要打开的文件路径
    AppendFile::AppendFile(const std::string &filename)
        // 初始化列表 (Initializer List)：比在函数体内赋值效率更高
        : buffer_{new char[FILE_BUFF_SIZE]}, // 1. 分配 1MB 的堆内存作为缓冲区，交给 unique_ptr 管理
          fp_{nullptr},                       // 2. 初始化文件指针为空
          writenBytes_{0}                     // 3. 初始化已写字节数为 0
    {
        // 检查缓冲区内存是否分配成功
        // 虽然 new 失败通常会抛异常 (std::bad_alloc)，但在某些嵌入式环境或设置下可能返回 nullptr
        if (!buffer_)
        {
            exit(EXIT_FAILURE); // 内存分配失败，直接终止程序 (EXIT_FAILURE 是标准宏)
        }

        // fopen：打开文件
        // 参数 1：filename.c_str() 将 C++ string 转为 C 风格字符串 (const char*)
        // 参数 2："a" 表示 Append（追加模式）
        //         - "r": 只读 (文件必须存在)
        //         - "w": 只写 (若文件存在则清空，不存在则创建)
        //         - "a": 追加 (若文件不存在则创建，写入时总是追加到末尾)
        fp_ = fopen(filename.c_str(), "a");

        // assert：断言宏
        // 作用：在 Debug 模式下，如果 fp_ 为 nullptr (文件打开失败)，程序会崩溃并打印错误信息
        // 注意：assert 在 Release 模式下会被编译器优化掉（不执行），所以不能用它处理运行时逻辑
        assert(fp_ != nullptr);

        // setbuffer：设置文件流的自定义缓冲区
        // 这是本类的核心性能优化点！
        // 默认情况下，FILE* 内部有一个很小的缓冲区（通常是 4KB 或 1KB）
        // 参数解释：
        //   - fp_: 文件指针
        //   - buffer_.get(): 获取 unique_ptr 管理的原始 char* 指针
        //   - FILE_BUFF_SIZE: 缓冲区大小 (1MB)
        // 效果：每次写入先写到这 1MB 的内存里，满了才真正调用一次系统调用写入磁盘，大大减少 IO 次数
        setbuffer(fp_, buffer_.get(), FILE_BUFF_SIZE);
    }

    // ================= 析构函数实现 =================
    // 功能：关闭文件、释放资源
    AppendFile::~AppendFile()
    {
        // fclose：关闭文件
        // 注意：fclose 内部会先调用 fflush，把缓冲区里残留的数据刷到磁盘，所以不用担心数据丢失
        if (fp_) {
            fclose(fp_);
        }
        
        fp_ = nullptr; // 悬空指针置空（虽然析构后对象就销毁了，这步是防御性编程习惯）

        // buffer_.reset()：显式释放 unique_ptr 管理的内存
        // 其实这一步是“多余”的：因为 buffer_ 是成员变量，对象析构时，buffer_ 的析构函数会自动调用，自动释放内存
        // 写在这里是为了代码逻辑的清晰性，强调资源释放的顺序
        buffer_.reset();
    }

    // ================= C++ 风格 append 重载 =================
    // 功能：接收 std::string，转发给 C 风格接口
    void AppendFile::append(const std::string &msg)
    {
        // msg.c_str(): 获取 string 内部的 C 字符串指针
        // msg.size(): 获取字符串的长度 (不包括末尾的 '\0')
        append(msg.c_str(), msg.size());
    }

    // ================= C 风格 append 实现 (核心逻辑) =================
    // 功能：确保将 len 字节的数据全部写入文件
    void AppendFile::append(const char *msg, const size_t len)
    {
        // 第一次尝试写入
        size_t n = write(msg, len); 
        
        // remain：剩余未写入的字节数
        size_t remain = len - n;

        // 为什么需要这个 while 循环？
        // 理论上 fwrite 要么全写完，要么报错。但在极少数情况下（如写入过程中被信号中断），
        // 或者对于非常规文件（如管道、Socket），可能会出现“部分写入”的情况。
        // 这个循环保证了数据的“原子性”：要么全写完，要么报错。
        while (remain > 0)
        {
            // 第二次及之后的写入：
            // msg + n: 指针偏移到上次没写完的位置
            // remain: 剩余长度
            size_t x = write(msg + n, remain);

            // 如果 x == 0，说明写入了 0 字节，通常意味着出错了
            if (x == 0)
            {
                // ferror：检查文件流的错误标志
                int err = ferror(fp_);
                if (err)
                {
                    // strerror(err): 将错误码转换为人类可读的字符串 (如 "No space left on device")
                    // fprintf(stderr, ...): 打印到标准错误流
                    fprintf(stderr, "appendFile::append failed %s \n", strerror(err));
                }
            }
            
            // 更新已写入的偏移量和剩余量
            n += x;
            remain = len - n;
        }

        // 全部写入成功后，更新总写入字节计数器
        writenBytes_ += n;
    }

    // ================= flush 函数实现 =================
    // 功能：强制将缓冲区数据写入磁盘
    void AppendFile::flush()
    {
        // fflush：刷新 FILE* 的缓冲区
        // 注意：即使我们设置了 1MB 的大缓冲区，有时候也需要立即看到日志（如程序崩溃前），
        // 这时就需要手动调用 flush。
        fflush(fp_);
    }

    // ================= getWriteBytes 函数实现 =================
    // 功能：获取当前已写入的总字节数
    size_t AppendFile::getWriteBytes() const
    {
        return writenBytes_;
    }

} // namespace logfile