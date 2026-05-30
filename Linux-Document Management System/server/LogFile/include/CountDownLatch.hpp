// ================= 头文件包含区 =================
// 引入互斥锁：用于保护共享数据（count_）
#include <mutex>
// 引入条件变量：用于线程间的等待/通知机制
#include <condition_variable>
// 引入原子变量（注：当前类实现中未使用，可能是预留或历史代码）
#include <atomic>

using namespace std;

// ================= 头文件保护 =================
#ifndef COUNT_DOWNLATCH_HPP
#define COUNT_DOWNLATCH_HPP

namespace logfile
{
    // ================= 倒计时门闩类定义 =================
    // 核心作用：线程同步工具
    // 典型场景：
    // 1. 主线程等待 N 个子线程全部完成初始化
    // 2. 等待某一个关键操作执行完毕（如之前 AsynLogging 中等待后台线程启动）
    class CountDownLatch
    {
    private:
        // ================= 私有成员变量 =================
        
        // 倒计时计数器
        // 含义：还需要等待 count_ 个事件发生
        int count_;

        // 互斥锁
        // mutable 关键字：非常关键！
        // 作用：允许在 const 成员函数（如 getCount()）中修改这个成员变量
        // 原因：互斥锁的 lock()/unlock() 是非 const 操作，但 getCount() 逻辑上是“只读”的，
        //       为了在 const 函数中也能加锁保护数据，必须将 mutex_ 声明为 mutable
        mutable std::mutex mutex_;

        // 条件变量
        // 作用：让线程阻塞等待，直到 count_ 变为 0 时被唤醒
        std::condition_variable cond_;

    public:
        // ================= 公有接口 =================

        // 构造函数
        // 参数 count：初始倒计时值（需要等待的事件数量）
        explicit CountDownLatch(int count);

        // 等待函数
        // 功能：阻塞当前线程，直到 count_ 减为 0
        void wait();

        // 倒计时减 1 函数
        // 功能：将 count_ 减 1，如果减到 0，则唤醒所有等待在 wait() 上的线程
        void countDown();

        // 获取当前倒计时值
        // const 修饰：表示该函数不会修改对象的逻辑状态（count_ 的值）
        int getCount() const;
    };
} // namespace logfile

#endif // 结束头文件保护