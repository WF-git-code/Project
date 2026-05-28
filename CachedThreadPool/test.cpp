#include "CachedThreadPool.hpp"
#include <iostream>
#include <vector>
#include <future>
#include <thread>
#include <atomic>
#include <chrono>

using namespace std;
using namespace std::chrono;

// 原子计数器，统计任务执行情况
std::atomic<long long> total_tasks{ 0 };
std::atomic<long long> success_tasks{ 0 };
std::atomic<long long> failed_tasks{ 0 };

long long cpu_task(int id) {
    volatile long long sum = 0;
    for (int i = 0; i < 10000; ++i) {
        sum += i * id;
    }
    success_tasks++;
    return sum;
}

int main() {
    std::cout << "===== CachedThreadPool 压力测试开始 =====" << std::endl;

    // 测试参数配置
    const int QUEUE_SIZE = 1000;
    const int THREAD_NUM = 8;
    const int WAIT_TIME = 100;
    const int TOTAL_TASKS = 100000;
    const int SUBMIT_THREADS = 4;

    CachedThreadPool pool(10,100,10,10,17);

    std::cout << "线程池配置：" << std::endl;
    std::cout << "  队列大小：" << QUEUE_SIZE << std::endl;
    std::cout << "  核心线程：" << THREAD_NUM << std::endl;
    std::cout << "  入队超时：" << WAIT_TIME << "微秒" << std::endl;
    std::cout << "  总任务数：" << TOTAL_TASKS << std::endl;
    std::cout << "  提交线程：" << SUBMIT_THREADS << std::endl << std::endl;

    auto start_time = high_resolution_clock::now();

    std::vector<std::thread> submitters;
    // 每个提交线程使用自己的vector，避免竞争
    std::vector<std::vector<std::future<long long>>> thread_futures(SUBMIT_THREADS);

    for (int t = 0; t < SUBMIT_THREADS; ++t) {
        submitters.emplace_back([&pool, &thread_futures, TOTAL_TASKS, SUBMIT_THREADS, t]() {
            int tasks_per_thread = TOTAL_TASKS / SUBMIT_THREADS;
            for (int i = 0; i < tasks_per_thread; ++i) {
                int task_id = t * tasks_per_thread + i;
                total_tasks++;

                auto fut = pool.Submit(cpu_task, task_id);
                thread_futures[t].push_back(std::move(fut));
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

    // 等待所有任务执行完成
    long long total_result = 0;
    for (auto& fut : futures) {
        try {
            total_result += fut.get();
        }
        catch (...) {
            failed_tasks++;
        }
    }
    //pool.Stop();
    pool.ForcedStop();

    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time);

    // 输出测试结果
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
        std::cout << std::endl << "测试通过！所有任务正常执行" << std::endl;
    }
    else {
        std::cout << std::endl << "测试失败！有任务未正常执行" << std::endl;
    }
    //std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}
