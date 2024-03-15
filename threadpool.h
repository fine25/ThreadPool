#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>
//Any���ͣ��ɽ���������������
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//�˹��캯��������Any���ͽ���������������
	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data)){}

	//��Any��������洢��data������ȡ����
	template<typename T>
	T cast_()
	{
		//��base_�ҵ�����ָ���Derive���󣬴�������ȡ��data��Ա����
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;

	}
private:
	//����
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	//������
	template<typename T>
	class Derive :public Base
	{
	public:
		Derive(T data):data_(data){}
		T data_;//����������������
	};
	//����һ������ָ��
	std::unique_ptr<Base> base_;
};

//ʵ��һ���ź�����
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
		,isExit_(false)
	{}
	~Semaphore()
	{
	isExit_ = true;
	}


	//��ȡһ���ź���Դ
	void wait()
	{
		if(isExit_)
	return;
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	//����һ���ź���Դ
	void post()
	{
		if(isExit_)
	return;
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}

private:
	std::atomic_bool isExit_;
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;

};

class Task;
//ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isvalid = true);
	~Result() = default;
	//��ȡ����ִ����ķ���ֵ
	void setValue(Any any);

	//get,�û����ô˷�����ȡtask�ķ���ֵ
	Any get();
private:
	Any any_;//�洢����ķ���ֵ
	Semaphore sem_;//�߳�ͨ���ź���
	std::shared_ptr<Task> task_;//ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;//����ֵ�Ƿ���Ч
};

//���������࣬�û����Զ��������������ͣ���Task�̳У���дrun����
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;
private:
	Result* result_;//Result�������������
};

//�̳߳�֧�ֵ�ģʽ
enum class PoolMode {
	MODE_FIXED,//�̶̹߳�����
	MODE_CACHED,//������̬����
};

//�߳�����
class Thread {
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	//�̹߳���
	Thread(ThreadFunc func);
	// �߳�����
	~Thread();
	
	//�����߳�
	void start();

	//��ȡ�߳�id
	int getId()const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;//�����߳�id
};

//�̳߳�����
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	//���������������ֵ
	void setTaskQueMaxThreshHold(int threshhold);

	//��������߳�������ֵ
	void setThreadSizeThreshHold(int threshhold);

	//�����̳߳ع���ģʽ
	void setMode(PoolMode mode);


	//�ύ����
		/*example:
		class MyTask:public Task{
		public:
		Any run();
			 };*/
	Result submitTask(std::shared_ptr<Task> sp);

	//�����̳߳�
	void start(int initThreadSize =4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//�����̺߳���
	void threadFunc(int threadid);
	
	//������е�״̬
	bool checkRunningState()const;


private:
	//�Ż�std::vector<Thread*> threads_;//�߳��б�
	//�Ż�std::vector<std::unique_ptr<Thread>> threads_;
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	int initThreadSize_;		//��ʼ���߳�����
	int threadSizeThreshHold_; //�߳�����������ֵ
	std::atomic_int curThreadSize_;//��¼��ǰ�߳�������
	std::atomic_int idleThreadSize_;//��¼�����߳�����

	std::queue<std::shared_ptr<Task> > taskQue_;//������У�������ָ���ֹ����������������ڶ̣���������
	std::atomic_int taskSize_;  //���������
	int taskQueMaxThreshHold_;	//�����������������ֵ

	std::mutex taskQueMtx_;		//��֤��������̰߳�ȫ
	std::condition_variable notFull_;//������в���
	std::condition_variable notEmpty_;//������в���
	std::condition_variable exitCond_;//�̳߳ػ���

	PoolMode poolMode_;//��ǰ�̳߳صĹ���ģʽ
	
	std::atomic_bool isPoolRunning;//��ǰ�̳߳�����״̬

};

#endif // !THREADPOOL_H

