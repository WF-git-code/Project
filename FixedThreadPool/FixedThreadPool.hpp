#pragma once
#include"TaskQueue1.hpp"
#include<functional>
#include<thread>
#include<future>
#include<atomic>
#include<iostream>
//核心思路：将任务封装为void(void)类型放入任务队列，线程池启动，调用excute添加
//无返回值无参数的任务，有参数或者有返回值，调用submit封装为无参无返回值的任务
//放入任务队列，线程池启动时构造函数调用CreatThread,构造线程的同时注册线程函数
//RunThreadPool(从任务队列拿取任务)，添加任务如果任务队列满的话，则当前线程直接执行
//确保不会丢失返回值
class FixedThreadPool {
public:
	using Task = std::function<void(void)>;
private:
	TaskQueue<Task> task_list;
	std::list<std::unique_ptr<std::thread>>ThreadList;
	std::atomic_bool IS_Runing;
	std::once_flag stop_once_flag;
	size_t ThreadNum;
	void Creat_Thread(size_t threadnum) {
		IS_Runing = true;
		for (int i = 0; i < threadnum; ++i) {
			ThreadList.push_back(std::make_unique<std::thread>(&FixedThreadPool::RunningThreadPool,this));
		}
	}
	void RunningThreadPool() {
		while (IS_Runing) {
			Task task;
			// 如果Take返回false，说明队列已经停止且为空，直接退出循环
			if (0!=task_list.Take(task)) {
				break;
			}
			// 只有当任务有效且线程池还在运行时才执行
			if (task && IS_Runing) {
				task();
			}
		}
	}
	void StopThreadPool() {
		// 第一步：先让队列停止接受新任务，等待所有现有任务执行完成
		task_list.Wait_Stop();
		// 第二步：所有任务都执行完了，再告诉工作线程可以退出了
		IS_Runing = false;
		// 第三步：join所有已经退出的工作线程
		for (auto& tha : ThreadList) {
			if (tha && tha->joinable()) {
				tha->join();
			}
		}
		ThreadList.clear();
	}
public:
	FixedThreadPool(size_t qusize = 100,size_t wait_time_s=10 ,size_t numsthreads = 8) :task_list(qusize,wait_time_s),ThreadNum(numsthreads), IS_Runing(false) {
		Creat_Thread(numsthreads);
	}
	~FixedThreadPool() {
		Stop();
	}
	void Stop() {
		std::call_once(stop_once_flag, [this]()->void {StopThreadPool(); });
	}
	void Excute(const Task& task) {
		if (task_list.Put(task) != 0) {
			std::cout << "Task Put Erro" << std::endl;
			task();
		}
	}
	void Excute(Task&& task) {
		if (task_list.Put(std::forward<Task>(task)) != 0) {
			std::cout << "Task Put Erro" << std::endl;
			task();
		}
	}
	// 提交带返回值的任务到线程池
	// 接收任意函数、任意个数参数
	// 返回 std::future<返回值类型>，调用 .get() 即可获取结果
	template<class Func, class...Args>
	auto Submit(Func&& func, Args&&...args) {
		// 1. 自动推导函数的返回值类型
		using RetType = decltype(func(args...));

		// 2. 用 packaged_task 包装带返回值的函数
		//    它会自动存储函数执行后的返回值
		//    用 shared_ptr 保证任务在线程执行期间不被销毁
		auto task = std::make_shared<std::packaged_task<RetType()>>(
			// 绑定函数和参数，变成无参可调用对象
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		);

		// 3. 获取 future 凭证，未来用它拿返回值
		std::future<RetType> result = task->get_future();
		//task此时是一个指向std::packaged_task<RetType()>的指针，该类型重载了（）,执行函数并把返回值存到其本身的对象中
		// 4. 把任务包装成线程池能识别的 void() 类型，提交到队列
		if (task_list.Put([task]() { (*task)(); }) != 0)
		{//lambda表达式讲执行这个对象的动作封装为void()类型的对象放入任务队列
			// 5. 入队失败（队列满/池关闭），当前线程立即执行任务
			//    保证任务绝不丢失
			(*task)();
		}

		// 6. 返回 future 给调用方
		return result;
	}


};