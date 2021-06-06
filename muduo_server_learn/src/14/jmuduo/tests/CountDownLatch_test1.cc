#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Thread.h>

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <string>
#include <stdio.h>

using namespace muduo;

class Test
{
 public:
  Test(int numThreads)
    : latch_(1),
      threads_(numThreads)
  {
    //创建3个线程
    for (int i = 0; i < numThreads; ++i)
    {
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      
      //线程的回调函数：threadFunc
      //线程的名称：name
      threads_.push_back(new muduo::Thread(
            boost::bind(&Test::threadFunc, this), muduo::string(name)));
    }
    //从begin到end执行：boost::bind(&Thread::start, _1)，用_1传递的参数来调用start
    //返回的是boost::function
    /*
    boost::bind(&Thread::start, _1)可用函数对象代替
    函数对象重载()运算符
    class foo
    {
      operator()(Thread* t)
    };
    */
    for_each(threads_.begin(), threads_.end(), boost::bind(&Thread::start, _1));
  }

  void run()
  {
    latch_.countDown();//将计数器减为0
  }

  void joinAll()
  {
    for_each(threads_.begin(), threads_.end(), boost::bind(&Thread::join, _1));
  }

 private:

  //三个线程都启动了，调用threadFunc
  void threadFunc()
  {
    latch_.wait();//三个线程都在等待主线程发号施令，等待计数器=0
    printf("tid=%d, %s started\n",
           CurrentThread::tid(),
           CurrentThread::name());

    

    printf("tid=%d, %s stopped\n",
           CurrentThread::tid(),
           CurrentThread::name());
  }

  CountDownLatch latch_;
  boost::ptr_vector<Thread> threads_;
};

int main()
{
  //主线程的PID和TID
  printf("pid=%d, tid=%d\n", ::getpid(), CurrentThread::tid());
  Test t(3);
  sleep(3);//sllep 3s，子线程是不运行的
  printf("pid=%d, tid=%d %s running ...\n", ::getpid(), CurrentThread::tid(), CurrentThread::name());
  t.run();//主线程发号师令
  t.joinAll();

  printf("number of created threads %d\n", Thread::numCreated());
}


