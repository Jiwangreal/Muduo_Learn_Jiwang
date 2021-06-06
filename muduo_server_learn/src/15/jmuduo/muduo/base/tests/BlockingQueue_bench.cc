#include <muduo/base/BlockingQueue.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Timestamp.h>

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <map>
#include <string>
#include <stdio.h>

//度量时间的类
class Bench
{
 public:
  Bench(int numThreads)
    : latch_(numThreads),
      threads_(numThreads)
  {
    for (int i = 0; i < numThreads; ++i)
    {
      char name[32];
      snprintf(name, sizeof name, "work thread %d", i);
      threads_.push_back(new muduo::Thread(
            boost::bind(&Bench::threadFunc, this), muduo::string(name)));
    }
    for_each(threads_.begin(), threads_.end(), boost::bind(&muduo::Thread::start, _1));
  }

  //生产产品，将时间添加到队列中
  void run(int times)
  {
    printf("waiting for count down latch\n");
    latch_.wait();
    printf("all threads started\n");
    for (int i = 0; i < times; ++i)
    {
      muduo::Timestamp now(muduo::Timestamp::now());
      queue_.put(now);//有界队列
      usleep(1000);//1000微妙，总共usleep：1000微妙*10000=10秒
    }
  }

  void joinAll()
  {
    for (size_t i = 0; i < threads_.size(); ++i)
    {
      queue_.put(muduo::Timestamp::invalid());
    }

    for_each(threads_.begin(), threads_.end(), boost::bind(&muduo::Thread::join, _1));
  }

 private:

  //消费者线程消费产品
  void threadFunc()
  {
    printf("tid=%d, %s started\n",
           muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());

    std::map<int, int> delays;
    latch_.countDown();
    bool running = true;
    while (running)
    {
      muduo::Timestamp t(queue_.take());
      muduo::Timestamp now(muduo::Timestamp::now());
      //如果时间合法，就计算时间差
      if (t.valid())
      {
        //delay的单位是微秒，delay=生产整个产品，到这个产品被消费的时间差
        int delay = static_cast<int>(timeDifference(now, t) * 1000000);
        // printf("tid=%d, latency = %d us\n",
        //        muduo::CurrentThread::tid(), delay);
        //delays统计某个时间差出现的个数
        ++delays[delay];
      }
      running = t.valid();//joinAll()生产一个非法的时间产品跳出循环
    }

  //遍历map容器，统计看下某个时间差出现的个数
    printf("tid=%d, %s stopped\n",
           muduo::CurrentThread::tid(),
           muduo::CurrentThread::name());
    for (std::map<int, int>::iterator it = delays.begin();
        it != delays.end(); ++it)
    {
      printf("tid = %d, delay = %d, count = %d\n",
             muduo::CurrentThread::tid(),
             it->first, it->second);
    }
  }

  muduo::BlockingQueue<muduo::Timestamp> queue_;
  muduo::CountDownLatch latch_;
  boost::ptr_vector<muduo::Thread> threads_;
};

int main(int argc, char* argv[])
{
  int threads = argc > 1 ? atoi(argv[1]) : 1;

  Bench t(threads);
  t.run(10000);//这里生产者线程只有1个
  t.joinAll();
}
