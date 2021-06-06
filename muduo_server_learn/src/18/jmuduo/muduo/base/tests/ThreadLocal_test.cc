#include <muduo/base/ThreadLocal.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/Thread.h>

#include <boost/noncopyable.hpp>
#include <stdio.h>

class Test : boost::noncopyable
{
 public:
  Test()
  {
    printf("tid=%d, constructing %p\n", muduo::CurrentThread::tid(), this);
  }

  ~Test()
  {
    printf("tid=%d, destructing %p %s\n", muduo::CurrentThread::tid(), this, name_.c_str());
  }

  const std::string& name() const { return name_; }
  void setName(const std::string& n) { name_ = n; }

 private:
  std::string name_;
};

muduo::ThreadLocal<Test> testObj1;
muduo::ThreadLocal<Test> testObj2;

void print()
{
  printf("tid=%d, obj1 %p name=%s\n",
         muduo::CurrentThread::tid(),
  //打印obj1的线程特定数据地址
	 &testObj1.value(),
         testObj1.value().name().c_str());
  //第一次输出obj2的线程特定数据地址
  //因为此时obj2的setName()没有调用，所以其name()是空的
  //testObj2.value()会构造一个对象，打印出来
  printf("tid=%d, obj2 %p name=%s\n",
         muduo::CurrentThread::tid(),
	 &testObj2.value(),
         testObj2.value().name().c_str());
}

void threadFunc()
{
  print();//子线程调用print()，且子线程构造的对象与第一次主线程构造的对象是不一样的！！
  testObj1.value().setName("changed 1");
  testObj2.value().setName("changed 42");
  print();
}

int main()
{
  //testObj1.value()调用new T()，就是new Test()，调用构造函数
  testObj1.value().setName("main one");
  print();
  muduo::Thread t1(threadFunc);
  t1.start();
  t1.join();
  testObj2.value().setName("main two");
  print();

  pthread_exit(0);//主线程退出，仅退出自己，return，exit也行
}
