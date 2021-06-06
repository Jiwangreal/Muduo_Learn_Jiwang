#include <muduo/base/Singleton.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/ThreadLocal.h>
#include <muduo/base/Thread.h>

#include <boost/bind.hpp>
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

//表示muduo::ThreadLocal<Test> 这个对象是单例的
//Test对象还是每个对象都有的，因为他是TSD
//value()是线程特定数据，muduo::Singleton<muduo::ThreadLocal<Test> >::instance()是单例对象，每个线程都访问的同一个对象，可以将其看成是一个全局变量
//muduo::Singleton<muduo::ThreadLocal<Test> >::instance().value()是每个线程所拥有的TSD，但是TSD对象每个线程都不一样
#define STL muduo::Singleton<muduo::ThreadLocal<Test> >::instance().value()

void print()
{
  printf("tid=%d, %p name=%s\n",
         muduo::CurrentThread::tid(),//线程id
         &STL,//第一次，一旦调用value()，则会调用Test的构造函数（将Test类型传递给了模板的T）
         STL.name().c_str());
}

void threadFunc(const char* changeTo)
{
  print();
  STL.setName(changeTo);
  sleep(1);
  print();
}

int main()
{
  //TSD对象STL
  STL.setName("main one");
  
  //创建2个线程
  muduo::Thread t1(boost::bind(threadFunc, "thread1"));
  muduo::Thread t2(boost::bind(threadFunc, "thread2"));
  t1.start();
  t2.start();
  t1.join();
  print();
  t2.join();
  pthread_exit(0);
}
