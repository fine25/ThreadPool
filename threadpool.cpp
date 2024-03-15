#include"threadpool.h"
#include<thread>
#include<iostream>
const int TASK_MAX_THRESHHOLD = 3;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;
//�̳߳ع���
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
//�̳߳�����
ThreadPool::~ThreadPool()
{
	isPoolRunning = false;

	// �ȴ��̳߳��������е��̷߳���  ������״̬������ & ����ִ��������
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//���������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threshhold) 
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshhold;
}
//��������߳�������ֵ
void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	threadSizeThreshHold_ = threshhold;
}

//�����̳߳ع���ģʽ
void ThreadPool::setMode(PoolMode mode) 
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

//�����̳߳�
void ThreadPool::start(int initThreadSize)
{
	//��������״̬
	isPoolRunning = true;
	//��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	//�����̶߳���
	for (int i = 0; i < initThreadSize_; i++) 
	{
		//����thread�̶߳���ʱ�����̺߳�������thread�̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//threads_.emplace_back(std::move(ptr));
	}	
	//���������߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize_++;//��¼��ʼ�����߳�����
	}
}



//�ύ����
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//����
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//�߳�ͨ�ţ��ȴ���������п���
	if (!notFull_.wait_for(lock,
		std::chrono::seconds(1),
		[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))	
	{
		std::cerr << "task queue is full,submit fail." << std::endl;
		return Result(sp,false);
	}

	//�п��࣬��������������
	taskQue_.emplace(sp);
	taskSize_++;

	if (poolMode_ == PoolMode::MODE_CACHED
		&&taskSize_>idleThreadSize_
		&& curThreadSize_<threadSizeThreshHold_)
	{
		std::cout << "new thread created..." << std::endl;
		//�������߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//�����߳�
		threads_[threadId]->start();
		idleThreadSize_++;
		curThreadSize_++;
	}


	//����֪ͨ����������߳�ִ������
	notEmpty_.notify_all();

	return Result(sp);
}


//�����̺߳���
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(isPoolRunning)//��Ϊ���ǽ���ȡһ��
	{
		
		std::shared_ptr<Task> task;
		{
			//����
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			
			std::cout << "tid:" << std::this_thread::get_id() << "���ڳ��Ի�ȡ����..." << std::endl;
			//cachedģʽ��Ҫ�����̣߳�����init�������գ�����ʱ��>10s��
			while (taskQue_.size() == 0)
			{
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					//������������ʱ����
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							//�Ƴ�����
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
					//�ȴ�notEmpty
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
			

			std::cout << "tid:" << std::this_thread::get_id() << "��ȡ����ɹ�..." << std::endl;
			
			idleThreadSize_--;
			//ȡ������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			
			//֪ͨ�����߳�һ��������
			if (taskQue_.size() > 0) 
			{
				notEmpty_.notify_all();
			}
			//t֪ͨ��������������
			notFull_.notify_all();

		}//�ͷ���

		//��ǰ�̸߳���ִ������
		if (task != nullptr)
		{
			//task->run();
			task->exec();
		}
		idleThreadSize_++;
		auto lastTime = std::chrono::high_resolution_clock().now();//����
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





///////////////////Task��
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


///////////////////�̷߳���ʵ��

 int Thread::generateId_=0;

//�̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{}
// �߳�����
Thread::~Thread()
{}

//�����߳�
void Thread::start()
{
	//����һ���߳���ִ��һ���̺߳���
	std::thread t(func_,threadId_);
	t.detach();
}
//��ȡ�߳�id
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
	//�洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post();//�����ź���
}

