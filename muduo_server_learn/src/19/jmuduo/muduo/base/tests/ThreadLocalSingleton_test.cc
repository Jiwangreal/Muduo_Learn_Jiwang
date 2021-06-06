#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/base/CurrentThread.h>
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

void threadFunc(const char* changeTo)
{
  printf("tid=%d, %p name=%s\n",
         muduo::CurrentThread::tid(),
         &muduo::ThreadLocalSingleton<Test>::instance(),//每个线程里面的Test对象
         muduo::ThreadLocalSingleton<Test>::instance().name().c_str());
  muduo::ThreadLocalSingleton<Test>::instance().setName(changeTo);
  printf("tid=%d, %p name=%s\n",
         muduo::CurrentThread::tid(),
         &muduo::ThreadLocalSingleton<Test>::instance(),
         muduo::ThreadLocalSingleton<Test>::instance().name().c_str());

  // no need to manually delete it
  // muduo::ThreadLocalSingleton<Test>::destroy();
}

int main()
{
  //每个线程都有一个单例对象muduo::ThreadLocalSingleton<Test>::instance()
  //每个线程都有1个Test对象
  //比较与src\18\jmuduo\muduo\base\tests\SingletonThreadLocal_test.cc实现方式，这里更好些
  muduo::ThreadLocalSingleton<Test>::instance().setName("main one");

  //子线程
  muduo::Thread t1(boost::bind(threadFunc, "thread1"));
  muduo::Thread t2(boost::bind(threadFunc, "thread2"));
  t1.start();
  t2.start();
  t1.join();
  printf("tid=%d, %p name=%s\n",
         muduo::CurrentThread::tid(),
         &muduo::ThreadLocalSingleton<Test>::instance(),
         muduo::ThreadLocalSingleton<Test>::instance().name().c_str());
  t2.join();

  pthread_exit(0);
}
