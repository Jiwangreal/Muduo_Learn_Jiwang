#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>
#include <boost/function.hpp>

class Thread
{
public:
	typedef boost::function<void ()> ThreadFunc;//返回类型为void，形参为空
	explicit Thread(const ThreadFunc& func);//explicit表示可以显示调用，阻止隐士地转换构造

	void Start();
	void Join();

	void SetAutoDelete(bool autoDelete);

private:
	static void* ThreadRoutine(void* arg);
	void Run();
	ThreadFunc func_;//通过构造函数传递进来
	pthread_t threadId_;
	bool autoDelete_;
};

#endif // _THREAD_H_
