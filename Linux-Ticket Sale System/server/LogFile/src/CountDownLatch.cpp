#include "CountDownLatch.hpp"

namespace logfile
{
    // ================= 构造函数实现 =================
    // 功能：初始化倒计时计数器
    // 参数 count：需要等待的事件数量
    CountDownLatch::CountDownLatch(int count)
        : count_(count) {} // 使用初始化列表直接初始化 count_

    // ================= wait 函数实现（等待逻辑） =================
    // 功能：阻塞当前线程，直到 count_ 减为 0
    void CountDownLatch::wait()
    {
        // 1. 加锁：保护共享变量 count_
        // std::unique_lock：RAII 风格的锁，自动管理锁的生命周期
        std::unique_lock<std::mutex> locker(mutex_);

        // 2. 循环检查条件（注意是 while 不是 if！）
        // 为什么用 while？防止“虚假唤醒”（Spurious Wakeup）
        // 虚假唤醒：即使没有调用 notify_all()，系统也可能因某些原因唤醒线程
        // 此时必须再次检查 count_ 是否真的为 0，否则会错误地继续执行
        while (count_ > 0)
        {
            // 3. 条件变量等待
            // cond_.wait(locker) 做了两件事：
            //   1. 自动释放 mutex_ 锁（让其他线程有机会修改 count_）
            //   2. 阻塞当前线程，直到被 notify_all()/notify_one() 唤醒
            // 被唤醒后，会自动重新获取 mutex_ 锁，然后继续循环检查条件
            cond_.wait(locker);
        }
    }

    // ================= countDown 函数实现（倒计时逻辑） =================
    // 功能：将倒计时减 1，若减到 0 则唤醒所有等待线程
    void CountDownLatch::countDown()
    {
        // 1. 加锁：保护 count_ 的修改操作
        std::unique_lock<std::mutex> locker(mutex_);

        // 2. 计数器减 1
        count_--;

        // 3. 如果计数器减到 0，说明所有等待的事件都完成了
        if (count_ == 0)
        {
            // 唤醒所有等待在 wait() 上的线程
            // 为什么用 notify_all 而不是 notify_one？
            // 因为可能有多个线程在 wait()，我们需要让它们都知道倒计时结束了
            cond_.notify_all();
        }
    }

    // ================= getCount 函数实现（获取当前计数） =================
    // 功能：线程安全地获取当前的倒计时值
    // const 修饰：逻辑上不修改对象状态
    int CountDownLatch::getCount() const
    {
        // 1. 加锁：即使是读取操作，也需要加锁保证线程安全
        // 注意：这里能对 mutex_ 加锁，是因为 mutex_ 被声明为 mutable
        // mutable 允许在 const 函数中修改 mutex_ 的状态（lock/unlock）
        std::unique_lock<std::mutex> locker(mutex_);

        // 2. 返回当前计数值
        return count_;
    }

} // namespace logfile