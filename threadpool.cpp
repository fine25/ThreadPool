#include"threadpool.h"
#include<thread>
#include<iostream>
const int TASK_MAX_THRESHHOLD = 3;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;
//线程池构造
ThreadPool::ThreadPool()
	:initThreadSize_(0),
	taskSize_(0),
	taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
	threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
	curThreadSize_(0),
	poolMode_(PoolMode::MODE_FIXED),
	isPoolRunning(false),
	idleThreadSize_(0)
{}
//线程池析构
ThreadPool::~ThreadPool()
{
	isPoolRunning = false;

	// 等待线程池里面所有的线程返回  有两种状态：阻塞 & 正在执行任务中
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//设置任务队列上限值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold) 
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshhold;
}
//设置最大线程上线阈值
void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	threadSizeThreshHold_ = threshhold;
}

//设置线程池工作模式
void ThreadPool::setMode(PoolMode mode) 
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

//开启线程池
void ThreadPool::start(int initThreadSize)
{
	//设置启动状态
	isPoolRunning = true;
	//初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	//创建线程对象
	for (int i = 0; i < initThreadSize_; i++) 
	{
		//创建thread线程对象时，把线程函数给到thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//threads_.emplace_back(std::move(ptr));
	}	
	//启动所有线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize_++;//记录初始空闲线程数量
	}
}



//提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//上锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//线程通信，等待任务队列有空余
	if (!notFull_.wait_for(lock,
		std::chrono::seconds(1),
		[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))	
	{
		std::cerr << "task queue is full,submit fail." << std::endl;
		return Result(sp,false);
	}

	//有空余，把任务放入队列中
	taskQue_.emplace(sp);
	taskSize_++;

	if (poolMode_ == PoolMode::MODE_CACHED
		&&taskSize_>idleThreadSize_
		&& curThreadSize_<threadSizeThreshHold_)
	{
		std::cout << "new thread created..." << std::endl;
		//创建新线程
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//启动线程
		threads_[threadId]->start();
		idleThreadSize_++;
		curThreadSize_++;
	}


	//进行通知，让其分配线程执行任务
	notEmpty_.notify_all();

	return Result(sp);
}


//定义线程函数
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(isPoolRunning)//因为不是仅仅取一次
	{
		
		std::shared_ptr<Task> task;
		{
			//上锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			
			std::cout << "tid:" << std::this_thread::get_id() << "正在尝试获取任务..." << std::endl;
			//cached模式下要回收线程（超过init数量回收，空闲时间>10s）
			while (taskQue_.size() == 0)
			{
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					//条件变量，超时返回
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							//移除对象
							threads_.erase(threadid);
							idleThreadSize_--;
							curThreadSize_--;
							std::cout << "threadid=" << std::this_thread::get_id() << " exit" << std::endl;
							return;
						}
					}
				}
				else
				{
					//等待notEmpty
					notEmpty_.wait(lock);
				}
				if (!isPoolRunning)
				{
					threads_.erase(threadid);
					idleThreadSize_--;
					curThreadSize_--;
					std::cout << "threadid=" << std::this_thread::get_id() << " exit" << std::endl;
					exitCond_.notify_all();
					return;
				}
			}
			

			std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功..." << std::endl;
			
			idleThreadSize_--;
			//取出任务
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			
			//通知其他线程一起来消费
			if (taskQue_.size() > 0) 
			{
				notEmpty_.notify_all();
			}
			//t通知生产者生产任务
			notFull_.notify_all();

		}//释放锁

		//当前线程负责执行任务
		if (task != nullptr)
		{
			//task->run();
			task->exec();
		}
		idleThreadSize_++;
		auto lastTime = std::chrono::high_resolution_clock().now();//更新
	}
	threads_.erase(threadid);
	idleThreadSize_--;
	curThreadSize_--;
	std::cout << "threadid=" << std::this_thread::get_id() << " exit" << std::endl;
	exitCond_.notify_all();
	return;
}



bool ThreadPool::checkRunningState()const
{
	return isPoolRunning;
}





///////////////////Task类
Task::Task():result_(nullptr)
{}
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setValue(run());
	}
}
void Task::setResult(Result* res)
{
	result_ = res;
}


///////////////////线程方法实现

 int Thread::generateId_=0;

//线程构造
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{}
// 线程析构
Thread::~Thread()
{}

//启动线程
void Thread::start()
{
	//创建一个线程来执行一个线程函数
	std::thread t(func_,threadId_);
	t.detach();
}
//获取线程id
int Thread::getId()const
{
	return threadId_;
}

////////////////////Result
Result::Result(std::shared_ptr<Task> task, bool isvalid)
	:task_(task),
	isValid_(isvalid)
{
	task->setResult(this);
}

Any Result::get()
{
	if (!isValid_)
		return " ";
	sem_.wait();
	return std::move(any_);
}
void Result::setValue(Any any)
{
	//存储task的返回值
	this->any_ = std::move(any);
	sem_.post();//增加信号量
}

