#pragma once
#include"TaskQueue1.hpp"
#include<iostream>
#include<atomic>
#include<functional>
#include<map>
#include<thread>
#include<chrono>
#include<future>
class CachedThreadPool {
public:
	using Task = std::function<void(void)>;
private:
	TaskQueue<Task>TaskList;
	std::vector<std::unique_ptr<std::thread>>CoreThreadPool;
	std::map<std::thread::id, std::unique_ptr<std::thread>>Diltation_Thread_Pool;
	std::condition_variable Thread_Exit;//线程销毁条件变量
	std::atomic_int MaxThreadSize;//最大线程数
	std::atomic_int Core_ThreadSize;//核心线程数
	std::atomic_int All_ThreadSize;//当前总线程数
	std::atomic<int> Free_ThreadSize;//空闲线程数
	std::atomic<bool> Running;
	std::once_flag Flag;
	std::atomic_int Thread_Wait_Time;//线程空闲时间
	std::mutex Mutex;//互斥锁
	void Start(int corethreads) {//将核心线程放入vector永不销毁
		Running = true;
		Core_ThreadSize = corethreads;
		All_ThreadSize += corethreads;
		for (int i = 0; i < corethreads; ++i) {
			std::unique_ptr<std::thread>ptr = std::make_unique<std::thread>(&CachedThreadPool::Running_Thread,this);
			CoreThreadPool.push_back(std::move(ptr));//采用移动语义
			++Free_ThreadSize;//空闲线程数++
		}
	}
	void Running_Thread() {
		auto Tid = std::this_thread::get_id();//获取当前线程的ID
		auto StartTime = std::chrono::high_resolution_clock::now();
		auto id = Diltation_Thread_Pool.find(Tid);
		if (id!= Diltation_Thread_Pool.end() &&!Running) {
			id->second->detach();
			return;
		}
		while (Running) {
			Task task;
			if (id != Diltation_Thread_Pool.end()) {
				if (TaskList.size() == 0) {
					auto now = std::chrono::high_resolution_clock::now();
					auto IntervalTime = duration_cast<std::chrono::seconds>(now - StartTime).count();
					std::unique_lock<std::mutex>locker(Mutex);
					if (IntervalTime >= Thread_Wait_Time && !Diltation_Thread_Pool.empty()) {
						//detach执行完任务在分离销毁
						id->second->detach();
						Diltation_Thread_Pool.erase(id);
						--All_ThreadSize;
						--Free_ThreadSize;
						std::cout << "Delete Free Thread ID: " << Tid << "当前空闲线程数 " << Free_ThreadSize << std::endl;
						Thread_Exit.notify_all();
						return;
					}
				}
			}
			if (0 == TaskList.Take(task)) {//如果取任务成功，执行任务并且重置开始时间
				if (task && Running) {
					--Free_ThreadSize;
					task();
					++Free_ThreadSize;
					if (id != Diltation_Thread_Pool.end()) {
						StartTime = std::chrono::high_resolution_clock::now();
					}
				}
			}
		}
	}
	void StopThreadPool() {	
		TaskList.Wait_Stop();
		std::unique_lock<std::mutex>locker(Mutex);
		Running = false;
		for (auto& x : CoreThreadPool) {
			if(x->joinable()){
				x->join();//分离核心线程  join()会阻塞
			}
		}
		CoreThreadPool.clear();
		Core_ThreadSize = 0;
		Thread_Wait_Time = 1;//将超时时间设为1s
	
		while (!Diltation_Thread_Pool.empty()) {//超过超时时间自动唤醒
			Thread_Exit.wait_for(locker, std::chrono::milliseconds(1));
		}
		Diltation_Thread_Pool.clear();
	}
	void AddNewThread() {
		std::unique_lock<std::mutex>locker(Mutex);
		if (All_ThreadSize < MaxThreadSize&&Free_ThreadSize<=0) {
	
			auto ptr = std::make_unique<std::thread>(&CachedThreadPool::Running_Thread, this);
			std::thread::id Tid = ptr->get_id();
			Diltation_Thread_Pool.emplace(Tid, std::move(ptr));
			++All_ThreadSize;//总线程数目++
			++Free_ThreadSize;
			std::cout << "Add New Thread" << std::endl;
			std::cout << "线程总数:" << All_ThreadSize << std::endl;
			std::cout << "核心线程数:" << Core_ThreadSize << std::endl;
			std::cout << "非核心线程数:" << Diltation_Thread_Pool.size() << std::endl;
		}
	}
public:
	CachedThreadPool(int coretsize = 10, int Tasksize = 100, size_t Task_waittime=10, size_t thread_waittime=10,size_t maxthread=17)
	:Core_ThreadSize(coretsize),TaskList(Tasksize,Task_waittime),Thread_Wait_Time(thread_waittime),MaxThreadSize(maxthread)
	{
		Running = false;
		Free_ThreadSize=0;
		All_ThreadSize = 0;
		Start(coretsize);
	}
	~CachedThreadPool() {
		Stop();
	}
	void Stop() {
		std::call_once(Flag, std::bind(&CachedThreadPool::StopThreadPool, this));
	}
	void ForcedStop() {//暴力停止，已经加入线程池的任务不会继续执行
		TaskList.Stop();
		Running = false;
		std::unique_lock<std::mutex>locker(Mutex);
		for (auto& X : CoreThreadPool) {//先分离核心线程
			X->join();
		}
		CoreThreadPool.clear();
		for (auto& x : Diltation_Thread_Pool) {
			x.second->join();

		}
		Diltation_Thread_Pool.clear();
	}

	template<class Func, class...Args>
	auto Submit(Func&& func, Args&&...args) {
		using RetType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RetType>result = task->get_future();
		if (TaskList.Put([task]() {(*task)(); }) != 0) {
			std::cout << "Put Err This Thread doing" << std::endl;
			(*task)();
		}
		AddNewThread();//当有一个任务无法被加入的时候触发添加线程函数
		return result;
	}
	void Excute(const Task& task) {
		if (TaskList.Put(task) != 0) {
			std::cout << "Not Add Task To TaskList" << std::endl;
			task();
		}
		AddNewThread();
	}
	void Excute(Task&& task) {
		if (TaskList.Put(std::forward<Task>(task))!= 0) {
			std::cout << "Not Add Task To TaskList" << std::endl;
			task();
		}
		AddNewThread();
	}
};
