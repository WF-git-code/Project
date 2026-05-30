#ifndef SCHERULEDThREADdPOOL_HPP
#define SCHERULEDThREADdPOOL_HPP

#include <functional>
#include <thread>
#include <atomic> //原子操作
#include <future>
#include <iostream>
#include "TimerManager.hpp"
#include <memory>
#include <chrono>
using namespace std;

class ScheduledThreadPool
{

private:
	TimerManager m_timers;

public:
	ScheduledThreadPool()
	{
		m_timers.init(-1);
	}
	~ScheduledThreadPool()
	{
	}
	template <class Func, class... Args>
	void excute(int val, int interval, Func &&func, Args &&...args)
	{
		auto task = std::make_shared<TimerCallback>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		auto te = std::make_shared<Timer>(interval);
		te->init();
		te->set_timer([task]
					  { (*task)(); }, val, interval);
		m_timers.add_timer(te);
	}
	template <class Func, class... Args>
	auto submit(int val, int interval, Func &&func, Args &&...args)
	{

		using RetType = decltype(func(args...));

		auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

		std::future<RetType> result = task->get_future();
		// Timer te(interval);
		auto te = std::make_shared<Timer>(interval);
		te->init();
		te->set_timer([task]
					  { (*task)(); }, val, interval);
		m_timers.add_timer(te);
		return result;
	}
};

#endif // !SCHERULEDThreadPool
