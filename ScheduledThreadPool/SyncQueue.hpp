#pragma once
#include<list>//list和双端队列都可以，要频繁的怎删改查
#include<deque>
#include<vector>
#include<mutex>
#include <cstddef>
#include<condition_variable>
template<class Task>//同样是一个模板类
class SyncQeue {

private:
	std::list<Task>my_qeue;//双端队列
	mutable std::mutex my_mutex;
	std::condition_variable my_notEmpty;//消费者
	std::condition_variable my_notFull;//生产者
	size_t my_maxsize;//任务上限
	size_t m_waitTime = 10;//等待时间


	bool m_Stop;
	bool IsFull()const { return my_qeue.size() >= my_maxsize; }//是否停止
	bool IsEmpty()const { return my_qeue.empty(); }
	std::condition_variable m_waitsStop;
	template<class F>
	int Add(F&& f) {//（引用型别未定义）//处理有返回值的线程函数
		std::unique_lock<std::mutex>locker(my_mutex);
		while (!m_Stop && IsFull()) {
			//my_notFull.wait(locker);
			if (std::cv_status::timeout == my_notFull.wait_for(locker, std::chrono::milliseconds(m_waitTime))) {
				return -1;
			}
		}
		if (m_Stop)return -2;
		my_qeue.push_back(std::forward<F>(f));//
		my_notEmpty.notify_all();//唤醒线程处理事件
		return 0;
	}
public:
	SyncQeue(size_t maxsize) :my_maxsize(maxsize), m_Stop(false) {}
	~SyncQeue() {

	}
	void Stop() {//强制停止  添加到list的任务可能没有执行
		{
			std::unique_lock<std::mutex>locker(my_mutex);//让锁的生命周期限制，出作用域解锁
			m_Stop = true;
		}
		my_notFull.notify_all();
		my_notEmpty.notify_all();
	}
	void WaitStop() {
		std::unique_lock<std::mutex>locker(my_mutex);
		while (!IsEmpty()) {
			//m_waitsStop.wait(locker);
			m_waitsStop.wait_for(locker, std::chrono::milliseconds(1));
		}
		m_Stop = true;
		my_notFull.notify_all();
		my_notEmpty.notify_all();
	}
	int Put(Task&& task) {//生产者
		return Add(std::forward<Task>(task));
	}

	int Put(const Task& task) {
		return Add(task);
	}
	int Take(Task& task) {	//消费者//钓鱼的方式取得一个任务
		std::unique_lock<std::mutex>locker(my_mutex);
		while (!m_Stop && IsEmpty()) {
			if (std::cv_status::timeout == my_notEmpty.wait_for(locker, std::chrono::seconds(1))) {
				return -1;
			}
		}
		if (m_Stop)return -2;
		task = my_qeue.front();
		my_qeue.pop_front();
		my_notFull.notify_all();//唤醒线程添加事件
		return 0;
	}
	int Take(std::list<Task>& tasklist) {	//消费者//一次全部取完
		std::unique_lock<std::mutex>locker(my_mutex);
		while (!m_Stop && IsEmpty()) {
			if (std::cv_status::timeout == my_notEmpty.wait_for(locker, std::chrono::seconds(1))) {
				return -1;
			}
		}
		if (m_Stop)return -2;
		tasklist = std::move(my_qeue);//移动语义
		my_notFull.notify_all();//唤醒线程添加事件
		return 0;
	}
	bool Empty()const {
		std::unique_lock<std::mutex>locker(my_mutex);
		return my_qeue.empty();
	}
	size_t Size()const {
		std::unique_lock<std::mutex>locker(my_mutex);
		return my_qeue.size();
	}
	bool FULL()const {
		std::unique_lock<std::mutex>locker(my_mutex);
		return my_qeue.size() >= my_maxsize;
	}
	size_t Cout()const {
		return my_qeue.size();
	}

};