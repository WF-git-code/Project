// 缓存式线程池头文件
//
// 这个文件实现了一个类似Java的CachedThreadPool
// 主要特点：
// 1. 核心线程：固定数量，常驻内存，不会被销毁
// 2. 非核心线程：动态创建，空闲超时后自动销毁
// 3. 任务队列：存储待处理的任务
// 4. 线程安全：使用互斥锁和原子变量保证线程安全

#pragma once

#include "TaskQueue.hpp"
#include <iostream>
#include <atomic>
#include <functional>
#include <map>
#include <thread>
#include <chrono>
#include <future>

class CachedThreadPool {
public:
    using Task = std::function<void(void)>;  // 任务类型定义

private:
    TaskQueue<Task> TaskList;  // 任务队列（存储待处理的任务）
    
    std::vector<std::unique_ptr<std::thread>> CoreThreadPool;  // 核心线程池（常驻线程）
    std::map<std::thread::id, std::unique_ptr<std::thread>> Diltation_Thread_Pool;  // 非核心线程池（动态线程，使用map按线程ID存储）
    
    std::condition_variable Thread_Exit;  // 线程销毁条件变量
    
    std::atomic_int MaxThreadSize;   // 最大线程数（原子变量，保证线程安全）
    std::atomic_int Core_ThreadSize; // 核心线程数（原子变量）
    std::atomic_int All_ThreadSize;  // 当前总线程数（原子变量）
    std::atomic<int> Free_ThreadSize; // 空闲线程数（原子变量）
    std::atomic<bool> Running;       // 线程池是否在运行（原子变量）
    std::once_flag Flag;             // 用于保证停止函数只执行一次
    std::atomic_int Thread_Wait_Time; // 非核心线程空闲等待时间（秒）
    std::mutex Mutex;                // 互斥锁（保护共享数据）

    // 启动核心线程
    // 将指定数量的核心线程放入CoreThreadPool中
    void Start(int corethreads) {
        Running = true;  // 标记线程池正在运行
        Core_ThreadSize = corethreads;  // 设置核心线程数
        All_ThreadSize += corethreads;  // 总线程数增加
        
        // 创建核心线程
        for (int i = 0; i < corethreads; ++i) {
            std::unique_ptr<std::thread> ptr = std::make_unique<std::thread>(&CachedThreadPool::Running_Thread, this);
            CoreThreadPool.push_back(std::move(ptr));  // 移动语义，避免拷贝
            ++Free_ThreadSize;  // 空闲线程数加1
        }
    }

    // 线程运行函数
    // 所有线程（核心和非核心）都执行这个函数
    void Running_Thread() {
        auto Tid = std::this_thread::get_id();  // 获取当前线程的ID
        auto StartTime = std::chrono::high_resolution_clock::now();  // 记录线程开始时间
        
        // 检查是否是非核心线程且线程池已停止
        auto id = Diltation_Thread_Pool.find(Tid);
        if (id != Diltation_Thread_Pool.end() && !Running) {
            id->second->detach();  // 分离线程（不等待结束）
            return;
        }

        // 线程主循环
        while (Running) {
            Task task;  // 存储待执行的任务
            
            // 判断：如果是非核心线程
            if (id != Diltation_Thread_Pool.end()) {
                // 如果任务队列为空，检查是否超时
                if (TaskList.size() == 0) {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto IntervalTime = std::chrono::duration_cast<std::chrono::seconds>(now - StartTime).count();
                    
                    std::unique_lock<std::mutex> locker(Mutex);
                    
                    // 如果空闲时间超过等待时间且非核心线程池不为空
                    if (IntervalTime >= Thread_Wait_Time && !Diltation_Thread_Pool.empty()) {
                        id->second->detach();  // 分离线程
                        Diltation_Thread_Pool.erase(id);  // 从map中移除
                        --All_ThreadSize;  // 总线程数减1
                        --Free_ThreadSize;  // 空闲线程数减1
                        std::cout << "Delete Free Thread ID: " << Tid << " 当前空闲线程数 " << Free_ThreadSize << std::endl;
                        Thread_Exit.notify_all();  // 通知等待的线程
                        return;  // 线程退出
                    }
                }
            }

            // 从任务队列中获取一个任务
            if (0 == TaskList.Take(task)) {
                if (task && Running) {
                    --Free_ThreadSize;  // 空闲线程数减1
                    task();  // 执行任务
                    ++Free_ThreadSize;  // 空闲线程数加1
                    
                    // 如果是非核心线程，重置开始时间
                    if (id != Diltation_Thread_Pool.end()) {
                        StartTime = std::chrono::high_resolution_clock::now();
                    }
                }
            }
        }
    }

    // 停止线程池
    void StopThreadPool() {
        TaskList.Wait_Stop();  // 先等待任务队列中的任务处理完
        
        std::unique_lock<std::mutex> locker(Mutex);
        Running = false;  // 标记线程池停止
        
        // 等待所有核心线程结束
        for (auto& x : CoreThreadPool) {
            if (x->joinable()) {
                x->join();  // join()会阻塞等待线程结束
            }
        }
        CoreThreadPool.clear();  // 清空核心线程池
        Core_ThreadSize = 0;  // 核心线程数置0
        Thread_Wait_Time = 1;  // 将超时时间设置为1秒
        
        // 等待非核心线程结束
        while (!Diltation_Thread_Pool.empty()) {
            Thread_Exit.wait_for(locker, std::chrono::milliseconds(1));  // 超时等待
        }
        Diltation_Thread_Pool.clear();  // 清空非核心线程池
    }

    // 添加新的非核心线程
    void AddNewThread() {
        std::unique_lock<std::mutex> locker(Mutex);
        
        // 条件：总线程数 < 最大线程数 且 没有空闲线程
        if (All_ThreadSize < MaxThreadSize && Free_ThreadSize <= 0) {
            // 创建新线程
            auto ptr = std::make_unique<std::thread>(&CachedThreadPool::Running_Thread, this);
            std::thread::id Tid = ptr->get_id();
            
            // 放入非核心线程池（map）
            Diltation_Thread_Pool.emplace(Tid, std::move(ptr));
            ++All_ThreadSize;  // 总线程数加1
            ++Free_ThreadSize;  // 空闲线程数加1
            
            std::cout << "Add New Thread" << std::endl;
            std::cout << "线程总数: " << All_ThreadSize << std::endl;
            std::cout << "核心线程数: " << Core_ThreadSize << std::endl;
            std::cout << "非核心线程数: " << Diltation_Thread_Pool.size() << std::endl;
        }
    }

public:
    // 构造函数
    // 参数：核心线程数、任务队列大小、任务等待时间、线程等待时间、最大线程数
    CachedThreadPool(int coretsize = 10, int Tasksize = 100, size_t Task_waittime = 10, size_t thread_waittime = 10, size_t maxthread = 17)
        : TaskList(Tasksize, Task_waittime), MaxThreadSize(maxthread), Core_ThreadSize(coretsize),
          All_ThreadSize(0), Free_ThreadSize(0), Running(false), Thread_Wait_Time(thread_waittime) {
        Start(coretsize);  // 启动核心线程
    }

    // 析构函数
    ~CachedThreadPool() {
        Stop();  // 停止线程池
    }

    // 停止线程池（保证只执行一次）
    void Stop() {
        std::call_once(Flag, std::bind(&CachedThreadPool::StopThreadPool, this));
    }

    // 强制停止线程池
    // 与Stop()不同：不会等待任务队列中的任务处理完
    void ForcedStop() {
        TaskList.Stop();  // 直接停止任务队列
        Running = false;  // 标记线程池停止
        
        std::unique_lock<std::mutex> locker(Mutex);
        
        // 等待核心线程结束
        for (auto& X : CoreThreadPool) {
            X->join();
        }
        CoreThreadPool.clear();
        
        // 等待非核心线程结束
        for (auto& x : Diltation_Thread_Pool) {
            x.second->join();
        }
        Diltation_Thread_Pool.clear();
    }

    // 提交任务（带返回值）
    // 模板参数：Func是函数类型，Args是参数类型
    template<class Func, class... Args>
    auto Submit(Func&& func, Args&&... args) {
        using RetType = decltype(func(args...));  // 推导返回值类型
        
        // 打包任务
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );
        
        std::future<RetType> result = task->get_future();  // 获取future对象
        
        // 尝试将任务放入队列
        if (TaskList.Put([task]() { (*task)(); }) != 0) {
            std::cout << "Put Err This Thread doing" << std::endl;
            (*task)();  // 如果队列满，当前线程直接执行
        }
        
        AddNewThread();  // 尝试添加新线程
        return result;  // 返回future对象
    }

    // 执行任务（const引用版本）
    void Excute(const Task& task) {
        if (TaskList.Put(task) != 0) {
            std::cout << "Not Add Task To TaskList" << std::endl;
            task();  // 如果队列满，当前线程直接执行
        }
        AddNewThread();  // 尝试添加新线程
    }

    // 执行任务（右值引用版本）
    void Excute(Task&& task) {
        if (TaskList.Put(std::forward<Task>(task)) != 0) {
            std::cout << "Not Add Task To TaskList" << std::endl;
            task();  // 如果队列满，当前线程直接执行
        }
        AddNewThread();  // 尝试添加新线程
    }
};
