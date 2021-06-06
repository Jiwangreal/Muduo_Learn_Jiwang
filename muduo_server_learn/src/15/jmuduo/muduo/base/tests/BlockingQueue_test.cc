#include <muduo/base/BlockingQueue.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Thread.h>

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <string>
#include <stdio.h>

class Test
{
 public:
  Test(int numThreads)
    : latch_(numThreads),
      threads_(numThreads)
  {
    //创建5个线程
    for (int i = 0; i < numThreads; ++i)
    {
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      //线程绑定的回调函数
      threads_.push_back(new muduo::Thread(
            boost::bind(&Test::threadFunc, this), muduo::string(name)));
    }
    //启动线程，调用线程的start
    for_each(threads_.begin(), threads_.end(), boost::bind(&muduo::Thread::start, _1));
  }

  void run(int times)
  {
    printf("waiting for count down latch\n");
    latch_.wait();
    printf("all threads started\n");
    //往无界队列中添加100个产品
    for (int i = 0; i < times; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "hello %d", i);
      queue_.put(buf);
      printf("tid=%d, put data = %s, size = %zd\n", muduo::CurrentThread::tid(), buf, queue_.size());
    }
  }

  void joinAll()
  {
    //往队列中添加5个stop产品
    for (size_t i = 0; i < threads_.size(); ++i)
    {
      queue_.put("stop");
    }

    for_each(threads_.begin(), threads_.end(), boost::bind(&muduo::Thread::join, _1));
  }

 private:

  void threadFunc()
  {
    printf("tid=%d, %s started\n",
           muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());

    latch_.countDown();//5个子线程运行完毕后，主线程才可以t.run(100);
    bool running = true;
    while (running)
    {
       //一旦产品添加进来，那么5个线程就可以去消费产品
      std::string d(queue_.take());
      printf("tid=%d, get data = %s, size = %zd\n", muduo::CurrentThread::tid(), d.c_str(), queue_.size());
      //joinAll的5个stop会分别被5个线程消费，所以5个线程分别会跳出循环，运行结束
      running = (d != "stop");//d=="stop"则跳出循环
    }

    printf("tid=%d, %s stopped\n",
           muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());
  }

  muduo::BlockingQueue<std::string> queue_;
  muduo::CountDownLatch latch_;
  boost::ptr_vector<muduo::Thread> threads_;//线程容器
};

int main()
{
  printf("pid=%d, tid=%d\n", ::getpid(), muduo::CurrentThread::tid());
  Test t(5);
  t.run(100);
  t.joinAll();

  printf("number of created threads %d\n", muduo::Thread::numCreated());
}
