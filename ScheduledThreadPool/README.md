# 高性能 C++ 计划线程池框架

## 项目介绍

本项目是一个基于 C++17 标准实现的高性能计划线程池框架，集成了**缓存线程池**和**定时任务线程池**两大核心组件。框架采用事件驱动模型，深度结合 Linux 系统原生特性，实现了高效的即时任务调度和高精度定时任务执行能力，适用于高并发后端服务、定时任务系统、异步处理引擎等多种场景。

## 技术栈

- C++17 标准
- Linux 系统调用（timerfd、epoll、pthread）
- Epoll IO 多路复用
- 智能指针与 RAII 资源管理
- 原子操作
- 条件变量与互斥锁
- 模板元编程

## 核心功能

### 缓存线程池

- 支持核心线程数和最大线程数可配置
- 智能动态线程管理：根据任务负载自动扩容，空闲线程超时自动回收
- 支持普通任务和带返回值的任务提交
- 有界任务队列防止内存溢出
- 提供正常关闭（等待所有任务完成）和强制关闭两种模式
- 支持任务队列满时的降级处理（调用者线程执行）

### 定时任务线程池

- 基于 Linux timerfd 和 Epoll 实现高精度定时（毫秒级精度）
- 支持一次性任务和周期性任务
- 任务执行与定时器分离：定时触发后将任务交给缓存线程池执行，避免阻塞定时器线程
- 自动移除已完成的一次性任务
- 支持动态添加和删除定时器

## 项目亮点

### 1. 任务调度与执行分离架构

- 定时器线程仅负责定时触发，不执行具体任务
- 所有任务执行都交给缓存线程池处理
- 保证定时精度不受任务执行时间影响
- 支持同时管理数千个定时器，资源消耗极低

### 2. 智能动态线程管理

- 核心线程常驻保证基础响应能力
- 任务队列满且无空闲线程时自动创建新线程
- 空闲线程超过指定时间自动回收
- 线程数量上限防止系统过载
- 原子操作保证线程计数的线程安全

### 3. 高精度定时能力

- 基于 Linux 内核 timerfd 实现，避免传统 sleep 定时的误差
- 采用单调时钟（CLOCK_MONOTONIC），不受系统时间调整影响
- 支持毫秒级的定时间隔配置
- Epoll IO 多路复用模型，单线程即可高效管理大量定时器

### 4. 完善的线程安全设计

- 任务队列采用互斥锁和条件变量实现线程安全
- 所有共享数据都使用原子操作或互斥锁保护
- 智能指针管理资源，避免内存泄漏
- RAII 机制保证资源正确释放
- 禁止拷贝构造和赋值，防止资源竞争

### 5. 灵活的任务提交接口

- 支持普通函数、lambda 表达式、成员函数等多种任务类型
- 支持任意数量和类型的参数传递
- 通过 std::future 获取任务执行结果
- 提供右值引用版本优化性能
- 模板化设计，接口简洁易用

### 6. 模块化可扩展设计

- 各个模块独立解耦：同步队列、定时器、线程池分离
- 接口设计清晰，易于维护和扩展
- 支持自定义任务类型和定时器实现
- 可与其他系统模块无缝集成

### 7. 优雅的关闭机制

- 正常关闭模式：等待所有已提交任务执行完成后再退出
- 强制关闭模式：立即停止所有线程，丢弃未执行任务
- 保证所有资源正确释放，避免程序异常退出
- 使用 std::call_once 保证关闭操作只执行一次

## 使用示例

### 缓存线程池使用

```c++
#include "CachedThreadPools.hpp"
#include <iostream>
#include <chrono>

using namespace std;
using namespace std::chrono;

int main() {
    // 创建缓存线程池，核心线程数2，任务队列大小200
    CachedThreadPool pool(2, 200);

    // 提交普通任务
    pool.excute([](){
        cout << "普通任务执行" << endl;
    });

    // 提交带返回值的任务
    auto future = pool.submit([](){
        this_thread::sleep_for(seconds(1));
        return 42;
    });
    cout << "带返回值任务结果: " << future.get() << endl;

    // 提交多个任务
    for(int i=0; i<10; ++i) {
        pool.excute([i](){
            cout << "任务" << i << "执行" << endl;
        });
    }

    // 正常关闭线程池，等待所有任务完成
    pool.Stop();
    return 0;
}
```

### 定时任务线程池使用

```c++
#include "ScheduledThreadPool.hpp"
#include <iostream>
#include <chrono>

using namespace std;
using namespace std::chrono;

int main() {
    // 创建定时任务线程池
    ScheduledThreadPool scheduler;

    // 提交一次性任务，2秒后执行
    scheduler.excute(2, 0, [](){
        cout << "一次性任务执行" << endl;
    });

    // 提交周期性任务，2秒后开始执行，每2秒执行一次
    scheduler.excute(2, 2, [](){
        static int count = 0;
        cout << "周期性任务执行，第" << ++count << "次" << endl;
    });

    // 提交带返回值的定时任务
    auto future = scheduler.submit(3, 0, [](){
        return "定时任务返回结果";
    });
    cout << future.get() << endl;

    // 保持程序运行
    while(true) {
        this_thread::sleep_for(seconds(1));
    }
    return 0;
}
```

## 编译运行

### 编译命令

```bash
g++ -std=c++17 Test.cpp -o threadpool -lpthread
```

### 运行程序

```bash
./threadpool
```

## 项目结构

```
.
├── CachedThreadPools.hpp  # 缓存线程池实现
├── ScheduledThreadPool.hpp # 定时任务线程池实现
├── SyncQueue.hpp          # 线程安全同步队列
├── Timer.hpp              # 定时器实现
├── TimerManager.hpp       # 定时器管理器
└── Test.cpp               # 测试代码
```