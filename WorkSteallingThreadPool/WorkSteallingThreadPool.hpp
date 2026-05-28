#pragma once
#include"TaskQueue2.hpp"
#include<functional>
#include<vector>
#include<iostream>
#include<thread>
#include<chrono>
#include<future>
class WorkStealingPool {
public:
	using Task = std::function<void(void)>;
private:
	TaskList<Task>Task_List;
	std::vector<std::unique_ptr<std::thread>>Thread_Group;
	size_t ThreadNum;
	std::atomic<bool> Running=true;
	std::once_flag Once_Flag;
	std::atomic<size_t> num{ 0 };
	int ThreadIndex() {
		return num++ % ThreadNum;
	}
	void Start(int numthread) {
		Running = true;
		for (int i = 0; i < numthread; ++i) {
			Thread_Group.push_back(std::make_unique<std::thread>(&WorkStealingPool::RunInThread, this, i));
		}
	}
	void RunInThread(const int index) {
		while (Running) {
			std::list<Task>tasklist;
			Task task;
			if (Task_List.Take(task, index) == 0) {
				task();
			}
			else {
				int nindex = ThreadIndex();
				while (nindex == index) {
					nindex = ThreadIndex();
				}
				if (index != nindex && Task_List.Take(tasklist, nindex) == 0) {
					std::cout <<nindex << " 桶任务偷取成功" << std::endl;
					for (auto& task : tasklist) {
						task();
					}
				}
				else {
					std::this_thread::yield();//cpu退出当前时间片
				}
			}
		}
	}
	void StopThreadGroup() {
		Task_List.Wait_Stop();
		Running = false;
		for (auto& tha : Thread_Group) {
			if (tha && tha->joinable()) {
				tha->join();
			}
		}
		Thread_Group.clear();
	}
public:
	WorkStealingPool( int Threadnum, size_t backetsize = 10, size_t maxtasksize = 100, size_t waittime = 10)
		:Task_List(Threadnum, maxtasksize, waittime), ThreadNum(Threadnum) {
		Start(Threadnum);
	}
	~WorkStealingPool() {
		Stop();
	}
	void Stop() {
		std::call_once(Once_Flag, [this] {StopThreadGroup(); });
	}
	template<class Func, class...Args>
	auto submit(Func&& func, Args&&...args) {

		using RetType = decltype(func(args...));

		auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

		std::future<RetType> result = task->get_future();

		if (Task_List.Put([task]() { (*task)(); }, ThreadIndex()) != 0)
		{
			(*task)();
		}
		return result;
	}
	void excute(const Task& task) {
		if (Task_List.Put(task, ThreadIndex()) != 0) {
			std::cout << "not add task queue..." << std::endl;
			task();
		}
	}
	void excute(Task&& task) {
		if (Task_List.Put(std::forward<Task>(task), ThreadIndex()) != 0) {
			std::cout << "not add task queue..." << std::endl;
			task();
		}
	}
};