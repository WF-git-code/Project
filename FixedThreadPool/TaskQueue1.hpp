#pragma once
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>

template <class Task>
class TaskQueue {
private:
    std::list<Task> TaskList;
    mutable std::mutex TaskList_mutex;

    std::condition_variable cv_not_empty; // 队列非空 → 有任务可以取
    std::condition_variable cv_not_full;  // 队列非满 → 有位置可以放

    size_t Max_Task;
    std::atomic<bool> stop{ false };
    size_t wait_time;

    // 只有持有锁时才调用，不需要加锁
    bool Is_Full() const { return TaskList.size() >= Max_Task; }
    bool Is_Empty() const { return TaskList.empty(); }

    template <class T>
    int Add(T&& task) {
        std::unique_lock<std::mutex> locker(TaskList_mutex);

        if (stop) return -2;

        //等待队列非满
        while (!stop && Is_Full()) {
            if (cv_not_full.wait_for(locker, std::chrono::microseconds(wait_time))
                == std::cv_status::timeout) {
                return -1;
            }
        }

        TaskList.push_back(std::forward<T>(task));
        cv_not_empty.notify_all(); // 添加任务后，通知队列非空
        return 0;
    }

public:
    TaskQueue(size_t max_task, size_t wait_time_s)
        : Max_Task(max_task), wait_time(wait_time_s) {
    }

    // stop检查在while循环内部
    int Take(Task& task) {
        std::unique_lock<std::mutex> locker(TaskList_mutex);

        //  永远在while循环里检查条件，防止虚假唤醒和死锁
        while (!stop && Is_Empty()) {
            cv_not_empty.wait(locker); //  等待队列非空
        }

        // 只有当停止且队列为空时，才返回失败
        if (stop && Is_Empty()) return 1;

        task = std::move(TaskList.front());
        TaskList.pop_front();
        cv_not_full.notify_all(); //  取出任务后，通知队列非满
        return 0;
    }

    // 批量Take同样修正
    int Take(std::list<Task>& tasklist) {
        std::unique_lock<std::mutex> locker(TaskList_mutex);

        while (!stop && Is_Empty()) {
            cv_not_empty.wait(locker);
        }

        if (stop && Is_Empty()) return 1;

        tasklist = std::move(TaskList);
        cv_not_full.notify_all();
        return 0;
    }

    int Put(const Task& task) {
        return Add(task);
    }

    int Put(Task&& task) {
        return Add(std::forward<Task>(task));
    }

    void Stop() {
        stop = true;
        cv_not_empty.notify_all();
        cv_not_full.notify_all();
    }

    // 完全重写，解决死锁
    void Wait_Stop() {
        // 1. 先设置停止标志，不持有锁
        stop = true;
        // 2. 唤醒所有等待的线程
        cv_not_empty.notify_all();
        cv_not_full.notify_all();

        // 3. 不持有锁等待队列变空
        while (!Is_Empty_T()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    bool Is_Empty_T() const {
        std::unique_lock<std::mutex> locker(TaskList_mutex);
        return TaskList.empty();
    }

    size_t size() const {
        std::unique_lock<std::mutex> locker(TaskList_mutex);
        return TaskList.size();
    }

    bool Is_Full_T() const {
        std::unique_lock<std::mutex> locker(TaskList_mutex);
        return TaskList.size() >= Max_Task;
    }
};