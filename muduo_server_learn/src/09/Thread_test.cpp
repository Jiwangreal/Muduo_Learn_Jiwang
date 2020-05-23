#include "Thread.h"
#include <boost/bind.hpp>
#include <unistd.h>
#include <iostream>
using namespace std;

class Foo
{
public:
	Foo(int count) : count_(count)
	{
	}

	void MemberFun()
	{
		while (count_--)
		{
			cout<<"this is a test ..."<<endl;
			sleep(1);
		}
	}

	void MemberFun2(int x)
	{
		while (count_--)
		{
			cout<<"x="<<x<<" this is a test2 ..."<<endl;
			sleep(1);
		}
	}

	int count_;
};

void ThreadFunc()
{
	cout<<"ThreadFunc ..."<<endl;
}

void ThreadFunc2(int count)
{
	while (count--)
	{
		cout<<"ThreadFunc2 ..."<<endl;
		sleep(1);
	}
}


int main(void)
{
	//创建线程对象，因为：typedef boost::function<void ()> ThreadFunc;//返回类型为void，形参为空
	Thread t1(ThreadFunc);//线程的执行体：ThreadFunc
	
	//下面的不符合接口转化，就用boost::bind进行转化，其返回的函数相当于：boost::function<vid()>
	//线程的执行体：ThreadFunc2
	Thread t2(boost::bind(ThreadFunc2, 3));//相当于返回了 boost::function<void ()> ，普通函数ThreadFunc2可以省略&符号
	Foo foo(3);//MemberFun会执行3次
	//线程的执行体：&Foo::MemberFun
	Thread t3(boost::bind(&Foo::MemberFun, &foo));//成员函数ThreadFunc2不可以省略&符号，&foo是第一个参数
	Foo foo2(3);
	Thread t4(boost::bind(&Foo::MemberFun2, &foo2, 1000));
	/*
	若将Foo foo2(3);注释掉，则：
	(&foo)->MemberFun();
	(&foo)->MemberFun2();
	两个对象随便调用了不同的成员函数，但是都访问了同一个count，相当于2个线程访问了共享变量count，会存在同步问题，两个线程会相互影响
	count的值，因为这里的线程不知道谁先谁后运行
	
	*/

	//有4个线程
	t1.Start();
	t2.Start();
	t3.Start();
	t4.Start();

	t1.Join();
	t2.Join();
	t3.Join();
	t4.Join();


	return 0;
}

