#include <muduo/base/ThreadPool.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/CurrentThread.h>

#include <boost/bind.hpp>
#include <stdio.h>

void print()
{
  printf("tid=%d\n", muduo::CurrentThread::tid());
}

void printString(const std::string& str)
{
  printf("tid=%d, str=%s\n", muduo::CurrentThread::tid(), str.c_str());
}

int main()
{
  //创建线程池
  muduo::ThreadPool pool("MainThreadPool");
  //启动5个线程
  pool.start(5);
  //添加任务
  pool.run(print);
  pool.run(print);
  for (int i = 0; i < 100; ++i)
  {
    char buf[32];
    snprintf(buf, sizeof buf, "task %d", i);
    //std::string(buf))传递函数参数
    pool.run(boost::bind(printString, std::string(buf)));
  }

  //计数值=1
  muduo::CountDownLatch latch(1);
  //目的是演示bind能绑定一个类的成员函数而已
  pool.run(boost::bind(&muduo::CountDownLatch::countDown, &latch));//计数-1
  latch.wait();//计数减为0，主线程等待倒计时
  pool.stop();//因为析构函数也会执行stop，是否会执行2次stop()?
              //不会，因为running_ = false;析构函数不会去执行的
}

