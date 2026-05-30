// ================= 头文件包含区 =================
// 引入 C 标准 I/O 库，用于底层文件操作 (FILE*, fopen, fwrite 等)
#include <stdio.h>

// 引入 C++ 标准字符串库，用于 std::string 类
#include <string>
// 引入 C++ 智能指针库，用于 std::unique_ptr (自动内存管理)
#include <memory>

// 使用标准命名空间 (注意：在头文件中 using namespace std; 通常不推荐，
// 容易引起命名冲突，但此处为了简化代码保留)
using namespace std;

// ================= 头文件保护 (Header Guard) =================
// 防止该头文件被同一个 .cpp 文件重复包含
#ifndef APPEND_FILE_HPP
#define APPEND_FILE_HPP

// ================= 命名空间 =================
// 定义命名空间 logfile，避免类名与其他库冲突
namespace logfile
{
    // ================= 类定义 =================
    // AppendFile：专门用于向文件“追加”(Append) 数据的类
    class AppendFile
    {
    private:
        // ================= 私有成员变量 =================
        
        // 静态常量：文件缓冲区大小，设为 1MB (1024 * 1024 字节)
        // static：所有该类对象共享此常量；const：值不可修改
        // size_t：无符号整数类型，专门用于表示“大小”或“长度”
        static const size_t FILE_BUFF_SIZE = 1024 * 1024; 

        // 智能指针：管理一个动态分配的字符数组 (缓冲区)
        // std::unique_ptr<char[]>：专门用于管理动态数组的智能指针
        // 好处：当对象析构时，会自动释放内存，防止内存泄漏
        std::unique_ptr<char[]> buffer_;

        // C 语言文件指针：用于操作底层文件
        FILE *fp_;

        // 计数器：记录已经写入文件的总字节数
        size_t writenBytes_;

        // ================= 私有成员函数 =================
        
        // 底层写入函数：真正执行写入逻辑的地方
        // 设为 private：封装内部实现细节，只允许内部调用
        // 参数 msg：数据指针；len：数据长度
        // 返回值：实际写入的字节数
        size_t write(const char *msg, const size_t len);

    public:
        // ================= 公有成员函数 (接口) =================
        
        // 构造函数：接收一个文件名，用于打开文件
        // explicit：禁止隐式类型转换 (防止 string 被隐式转成 AppendFile)
        // 参数 filename：要打开的文件路径
        explicit AppendFile(const std::string &filename); 

        // 析构函数：对象销毁时自动调用
        // 作用：关闭文件 (fclose)，释放资源 (buffer_ 由 unique_ptr 自动释放)
        ~AppendFile();

        // C++ 风格接口：追加一个 std::string
        // 参数 msg：要写入的字符串对象
        void append(const std::string &msg);            

        // C 语言风格接口：追加一个字符数组 (兼容 C 风格代码)
        // 参数 msg：字符指针；len：要写入的字节数
        void append(const char *msg, const size_t len); 

        // 强制刷新缓冲区：将缓冲区里的数据立即写入磁盘
        // 用途：防止数据留在内存中丢失
        void flush();

        // 获取已写入的总字节数
        // const 修饰：表示该函数“只读”，不会修改类的成员变量
        size_t getWriteBytes() const;
    };
} // namespace logfile

#endif // 结束头文件保护