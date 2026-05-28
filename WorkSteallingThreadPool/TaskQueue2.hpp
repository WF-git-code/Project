#pragma once
#include<list>
#include<vector>
#include<mutex>
#include<condition_variable>
#include<memory>
#include<atomic>

template<class Task>

class TaskList {
private:
	typedef struct Task_One {
		std::list<Task> Task_List_One;
		bool Is_Add=true;
	}Task_One;
	std::vector<Task_One>Task_List;
	size_t BacketSize;
	size_t MaxSize;//每个桶最大任务数
	std::unique_ptr<std::mutex[]>MutexVector;
	std::condition_variable NotEmpty;
	std::condition_variable NotFull;
	std::condition_variable WaitStop;
	size_t WaitTime;
	std::atomic<bool>StopFlag;
	size_t TotalTaskCount() {
		size_t TaskNum = 0;
		for (const auto& x : Task_List) {
			TaskNum += x.size();
		}
		return TaskNum;
	}
	bool IsFull(int index)const { return Task_List[index].Task_List_One.size() >= MaxSize; }
	bool IsEmpty(int index)const { return Task_List[index].Task_List_One.empty(); }
	template<class F>
	int Add(F&&task,int index) {
		std::unique_lock<std::mutex>locker(MutexVector[index]);
		if (Task_List[index].Is_Add) {
			while (!StopFlag && IsFull(index)) {
				if (std::cv_status::timeout == NotFull.wait_for(locker, std::chrono::milliseconds(WaitTime))) {
					return -1;
				}
			}
			if (StopFlag)return -2;
			Task_List[index].Task_List_One.push_back(std::forward<F>(task));
			NotEmpty.notify_all();//唤醒消费者
			return 0;
		}
		else {
			return -2;
		}
	}
public:
	TaskList(size_t backetsize = 10, size_t maxtasksize = 100, size_t waittime = 10):
	BacketSize(backetsize),MaxSize(maxtasksize),WaitTime(waittime)
	{
		Task_List.resize(BacketSize);
		MutexVector = std::make_unique<std::mutex[]>(BacketSize);
	}
	~TaskList() {

	}
	void ForceStop() {
		StopFlag = true;
		NotEmpty.notify_all();
		NotFull.notify_all();
	}
	void Wait_Stop() {
		for (int i = 0; i < BacketSize; ++i) {
			std::unique_lock<std::mutex>locker(MutexVector[i]);
			Task_List[i].Is_Add = false;
			while (!IsEmpty(i)) {
				WaitStop.wait_for(locker, std::chrono::milliseconds(1));
			}
			NotEmpty.notify_all();
			NotFull.notify_all();
		}
		StopFlag = true;
	}
	int Put(Task&& task, int index) {
		return Add(std::forward<Task>(task), index);
	}
	int Put(const Task& task, int index) {
		return Add(task, index);
	}
	int Take(Task& task, int index) {
		std::unique_lock<std::mutex>locker(MutexVector[index]);
		while (!StopFlag && IsEmpty(index)) {
			if (std::cv_status::timeout == NotEmpty.wait_for(locker, std::chrono::milliseconds(1))) {
				return -1;
			}
		}
		if (StopFlag)return -2;
		task = Task_List[index].Task_List_One.front();
		Task_List[index].Task_List_One.pop_front();
		NotFull.notify_all();
		return 0;
	}
	int Take(std::list<Task>& tasklist, int index) {
		std::unique_lock<std::mutex>locker(MutexVector[index]);
		while (!StopFlag && IsEmpty(index)) {
			if (std::cv_status::timeout == NotEmpty.wait_for(locker, std::chrono::milliseconds(1))) {
				return -1;
			}
		}
		if(StopFlag)return -2;
		tasklist=std::move(Task_List[index].Task_List_One);
		NotFull.notify_all();
		return 0;
	}
	size_t Count()const {
		return TotalTaskCount();
	}
	size_t BucketSize(int index)const {
		std::unique_lock<std::mutex>locker(MutexVector[index]);
		return Task_List[index].Task_List_One.size();
	}
	bool BucketFULL(int index)const {
		std::unique_lock<std::mutex>locker(MutexVector[index]);
		return Task_List[index].Task_List_One.size() >= BacketSize;
	}
	bool BucketEmpty(int index)const {
		std::unique_lock<std::mutex>locker(MutexVector[index]);
		Task_List[index].Task_List_One.empty();
	}

};
