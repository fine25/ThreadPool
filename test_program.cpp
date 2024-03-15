#include<iostream>
#include<chrono>
#include<string>
#include<thread>
#include"threadpool.h"
class MyTask :public Task {
public:
	MyTask(double begin, double end) :begin_(begin), end_(end)
	{}
	Any run()
	{
		double sum = 0;
		 std::this_thread::sleep_for(std::chrono::seconds(3));
		for (double i = begin_; i <= end_; i++)
		{

			sum += i;
		}
		return sum;
	}
private:
	double begin_;
	double end_;
};
int main()
{
	{
		ThreadPool pool;
		//用户自己设定
	//	pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(101.0, 200.0));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(201.0, 300.0));
		Result res4 = pool.submitTask(std::make_shared<MyTask>(301.0, 400.0));
		Result res5 = pool.submitTask(std::make_shared<MyTask>(401.0, 500.0));
		pool.submitTask(std::make_shared<MyTask>(401.0, 500.0));
		pool.submitTask(std::make_shared<MyTask>(401.0, 500.0));
		double num1 = res1.get().cast_<double>();
		double num2 = res2.get().cast_<double>();
		double num3 = res3.get().cast_<double>();
		double num4 = res4.get().cast_<double>();
		double num5 = res5.get().cast_<double>();
		std::cout << double(num1 + num2 + num3 + num4 + num5) << std::endl;
	}
	getchar();

	//std::this_thread::sleep_for(std::chrono::seconds(3));
	return 0;
}
