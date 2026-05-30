#pragma once
#include"SyncQueue.hpp"
#include<iostream>
#include<atomic>
#include<functional>
#include<map>
#include<thread>
#include<chrono>
#include<future>
using namespace std;
using namespace std::chrono;
class CachedThreadPool {
public:
	using Task = std::function<void(void)>;
private:
	SyncQeue<Task>m_queue;
	std::map < std::thread::id, std::unique_ptr<std::thread>>m_threadgroup;
	std::condition_variable m_threadExit;
	std::atomic_int m_maxThreadSize;//线程上限
	std::atomic_int m_coreThreadSize;//线程下限
	std::atomic<int>m_curThreadSize;//线程总数
	std::atomic<int>m_idleThreadSize;//空闲线程数
	std::atomic_bool m_running;
	std::once_flag flag;
	std::atomic_int m_maxIdleTime;//60s  空闲线程存活时间
	std::mutex m_mutex;
	void Start(int numthreads) {
		m_running = true;
		m_curThreadSize = numthreads;
		for (int i = 0; i < numthreads; ++i) {
			//auto tha = std::unique_ptr<std::thread>(new std::thread(&CachedThreadPool::RunInThread, this));
			auto tha = std::make_unique<std::thread>(&CachedThreadPool::RunInThread, this);
			std::thread::id tid = tha->get_id();//auto tid=tha->get_id();
			m_threadgroup.emplace(tid, std::move(tha));
			++m_idleThreadSize;
		}
	}
	void RunInThread() {
		auto tid = std::this_thread::get_id();
		auto startTime = std::chrono::high_resolution_clock::now();
		while (m_running) {
			Task task;//定义了一个Task类型的task     (void(void))
			if (m_queue.Size() == 0) {
				auto now = std::chrono::high_resolution_clock::now();
				auto intervalTime = duration_cast<std::chrono::seconds>(now - startTime).count();
				std::unique_lock<std::mutex>locker(m_mutex);
				if (intervalTime >= m_maxIdleTime && m_curThreadSize > m_coreThreadSize) {
					m_threadgroup.find(tid)->second->detach();
					m_threadgroup.erase(tid);
					--m_curThreadSize;
					--m_idleThreadSize;
					cout << "delete" << endl;
					cout << "idle Thread size  " << m_idleThreadSize << endl << "cur_thread_size   " << m_curThreadSize << endl;
					m_threadExit.notify_all();
					return;
				}

			}
			if (0 == m_queue.Take(task)) {//通过my_queue对象调用Take函数，因为Take函数参数为引用形式，直接将任务队列list第一个任务给task;
				if (task && m_running) {//如果task为真并且线程池运行状态为运行态的时候
					--m_idleThreadSize;
					task();//运行task（）任务即调用线程执行task任务
					++m_idleThreadSize;
					startTime = std::chrono::high_resolution_clock::now();
				}
			}
		}

	}
	void StopThreadGroup() {
		m_queue.WaitStop();
		m_coreThreadSize = 0;
		m_maxIdleTime = 1;
		std::unique_lock<std::mutex>locker(m_mutex);
		while (!m_threadgroup.empty()) {
			m_threadExit.wait_for(locker, std::chrono::milliseconds(1));
		}

		m_running = false;
		//m_threadgroup.clear();
	}
	void AddnewThread() {
		std::lock_guard<std::mutex>locker(m_mutex);
		if (m_idleThreadSize <= 0 && m_curThreadSize < m_maxThreadSize) {
			auto tha = std::make_unique<std::thread>(&CachedThreadPool::RunInThread, this);
			std::thread::id tid = tha->get_id();//auto tid=tha->get_id();
			m_threadgroup.emplace(tid, std::move(tha));
			++m_idleThreadSize;
			++m_curThreadSize;
			cout << "Add new thread" << endl;
			cout << "idle thread size" << m_idleThreadSize<< endl;
			cout << "cui thread size" << m_curThreadSize << endl;
		}
	}
public:
	CachedThreadPool(int numthreads = 2, int qusize = 200) :m_queue(qusize) {
		m_maxThreadSize =static_cast<size_t> (thread::hardware_concurrency() + 1);
		m_coreThreadSize = 2;
		m_maxIdleTime = 5;
		m_curThreadSize = 0;
		m_idleThreadSize = 0;
		m_running = false;
		Start(numthreads);
	}

	~CachedThreadPool() {
		Stop();
	}
	void Stop() {
		std::call_once(flag, [this]() {StopThreadGroup(); });
	}
	void ForcedStop()
	{
		m_queue.Stop();
		m_running = false;
		std::unique_lock<std::mutex> locker(m_mutex);
		auto it = m_threadgroup.begin();
		for (; it != m_threadgroup.end(); )
		{
			//it->second->join();
			it->second->detach();
			m_threadgroup.erase(it++);
		}
	}
	template<class Func, class...Args>
	auto submit(Func&& func, Args&&...args) {
	
		using RetType = decltype(func(args...));
		
		auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		
		std::future<RetType> result = task->get_future();//将rsult和被包装的task关联起来，一个任务唯一对应一个future

		if (m_queue.Put([task]() { (*task)(); }) != 0)//将该智能指针解引用+（）本质是运行里面的无参无返回值（参数已被绑定好）函数添加到任务队列中
		{
			cout << "not add task queue..." << endl;
			(*task)();
		}
		AddnewThread();
		return result;//返货线程返回的结果

	}
	void excute(const Task& task) {
		if (m_queue.Put(task) != 0) {
			cout << "not add task queue..." << endl;
			task();
		}
		AddnewThread();
	}
	void excute(Task&& task) {
		if (m_queue.Put(std::forward<Task>(task)) != 0) {
			cout << "not add task queue..." << endl;
			task();
		}
		AddnewThread();
	}

};

