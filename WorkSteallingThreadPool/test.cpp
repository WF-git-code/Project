#include "WorkSteallingThreadPool.hpp"
#include <iostream>
#include <vector>
#include <future>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
using namespace std;
using namespace std::chrono;

// 原子计数器，统计任务执行情况
std::atomic<long long> total_tasks{ 0 };
std::atomic<long long> success_tasks{ 0 };
std::atomic<long long> failed_tasks{ 0 };

// 短耗时任务：轻量计算，执行极快
long long short_task(int id)
{
    volatile long long sum = 0;
    for (int i = 0; i < 1000; ++i)
    {
        sum += i * id;
    }
    success_tasks++;
    return sum;
}

// 长耗时任务：重度计算+短暂休眠，模拟阻塞耗时任务
long long long_task(int id)
{
    volatile long long sum = 0;
    // 重度循环计算
    for (int i = 0; i < 100000; ++i)
    {
        sum += i * id;
    }
    // 模拟业务耗时，制造线程负载不均
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    success_tasks++;
    return sum;
}

int main() {
    std::cout << "===== WorkStealingPool 长短任务混合压力测试 =====" << std::endl;

    // 测试参数配置
    const int THREAD_NUM = 10;          // 线程数 = 桶数
    const int MAX_TASK_SIZE = 1000;     // 每个桶最大任务数（扩容避免阻塞）
    const int WAIT_TIME = 10;           // 等待时间
    const int TOTAL_TASKS = 100000;     // 总任务数
    const int SUBMIT_THREADS = 4;       // 提交线程数
    const int LONG_TASK_RATIO = 20;     // 长任务占比 20%，制造负载不均

    // 工作窃取线程池初始化
    WorkStealingPool pool(THREAD_NUM, THREAD_NUM, MAX_TASK_SIZE, WAIT_TIME);

    std::cout << "线程池配置：" << std::endl;
    std::cout << "  线程数：" << THREAD_NUM << std::endl;
    std::cout << "  每桶最大任务：" << MAX_TASK_SIZE << std::endl;
    std::cout << "  总任务数：" << TOTAL_TASKS << std::endl;
    std::cout << "  提交线程：" << SUBMIT_THREADS << std::endl;
    std::cout << "  长耗时任务占比：" << LONG_TASK_RATIO << "%" << std::endl << std::endl;

    auto start_time = high_resolution_clock::now();

    std::vector<std::thread> submitters;
    std::vector<std::vector<std::future<long long>>> thread_futures(SUBMIT_THREADS);

    // 随机数生成，随机分发长短任务
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> ratio(1, 100);

    for (int t = 0; t < SUBMIT_THREADS; ++t) {
        submitters.emplace_back([&pool, &thread_futures, &ratio, &gen, TOTAL_TASKS, SUBMIT_THREADS, t]() {
            int tasks_per_thread = TOTAL_TASKS / SUBMIT_THREADS;
            for (int i = 0; i < tasks_per_thread; ++i) {
                int task_id = t * tasks_per_thread + i;
                total_tasks++;

                // 按比例随机生成长短任务，制造线程负载不均衡，触发工作窃取
                if (ratio(gen) <= LONG_TASK_RATIO)
                {
                    auto fut = pool.submit(long_task, task_id);
                    thread_futures[t].push_back(std::move(fut));
                }
                else
                {
                    auto fut = pool.submit(short_task, task_id);
                    thread_futures[t].push_back(std::move(fut));
                }
            }
            });
    }

    // 等待所有提交线程完成
    for (auto& t : submitters) {
        t.join();
    }
    std::cout << "所有任务提交完成，等待执行..." << std::endl;

    // 合并所有future
    std::vector<std::future<long long>> futures;
    for (auto& tf : thread_futures) {
        futures.insert(futures.end(),
            std::make_move_iterator(tf.begin()),
            std::make_move_iterator(tf.end()));
    }

    // 等待所有任务执行完成，捕获异常
    long long total_result = 0;
    for (auto& fut : futures) {
        try {
            total_result += fut.get();
        }
        catch (...) {
            failed_tasks++;
        }
    }

    // 安全停止线程池
    pool.Stop();

    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time);

    // 输出详细测试结果
    std::cout << std::endl << "===== 测试结果 =====" << std::endl;
    std::cout << "总耗时：" << duration.count() << "毫秒" << std::endl;
    std::cout << "总任务数：" << total_tasks << std::endl;
    std::cout << "成功执行：" << success_tasks << std::endl;
    std::cout << "入队失败：" << (total_tasks - success_tasks - failed_tasks) << std::endl;
    std::cout << "异常失败：" << failed_tasks << std::endl;
    std::cout << "吞吐量：" << (total_tasks * 1000.0 / duration.count()) << "任务/秒" << std::endl;
    std::cout << "平均单任务耗时：" << (duration.count() * 1.0 / total_tasks) << "毫秒" << std::endl;
    std::cout << "计算结果：" << total_result << std::endl;

    if (success_tasks == total_tasks && failed_tasks == 0) {
        std::cout << std::endl << "测试通过！所有任务正常执行，工作窃取机制生效" << std::endl;
    }
    else {
        std::cout << std::endl << "测试失败！有任务未正常执行" << std::endl;
    }

    return 0;
}
