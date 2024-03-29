//#include <muduo/base/CountDownLatch.h>//多余不需要
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Timestamp.h>

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <vector>
#include <stdio.h>

using namespace muduo;
using namespace std;

MutexLock g_mutex;
vector<int> g_vec;
const int kCount = 10*1000*1000;

void threadFunc()
{
  for (int i = 0; i < kCount; ++i)
  {
    MutexLockGuard lock(g_mutex);//MutexLockGuard在构造函数中lock，在析构函数中unlock
    g_vec.push_back(i);
  }
}

int main()
{
  
  const int kMaxThreads = 8;
  g_vec.reserve(kMaxThreads * kCount);//预留8千万个整数=3亿bit，相当于300M左右的内存


  //没有锁的测试
  Timestamp start(Timestamp::now());//登记时间戳
  for (int i = 0; i < kCount; ++i)
  {
    g_vec.push_back(i);
  }

  printf("single thread without lock %f\n", timeDifference(Timestamp::now(), start));//插入1千万个整数，看需要多少时间


  //有锁的测试
  //下面的这个是主线程
  start = Timestamp::now();
  threadFunc();
  printf("single thread with lock %f\n", timeDifference(Timestamp::now(), start));

  //下面的是创建子线程
  for (int nthreads = 1; nthreads < kMaxThreads; ++nthreads)
  {
    //ptr_vector是指针vector，里面存放的是Thread的指针
    boost::ptr_vector<Thread> threads;//释放时，能够销毁存放在vector的对象
    g_vec.clear();
    start = Timestamp::now();

    //只有1个线程时，依次类推
    for (int i = 0; i < nthreads; ++i)
    {
      threads.push_back(new Thread(&threadFunc));
      threads.back().start();
    }
    //等待该线程结束。以此类推
    for (int i = 0; i < nthreads; ++i)
    {
      threads[i].join();
    }
    //看看只有一个线程，用了多少时间
    //以此类推，看看有两个线程，用了多少时间
    printf("%d thread(s) with lock %f\n", nthreads, timeDifference(Timestamp::now(), start));
  }
}

