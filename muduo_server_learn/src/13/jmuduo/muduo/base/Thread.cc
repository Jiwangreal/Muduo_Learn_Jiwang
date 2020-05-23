// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Thread.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/Exception.h>
//#include <muduo/base/Logging.h>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

namespace muduo
{
namespace CurrentThread
{
  // __thread修饰的变量是线程局部存储的。多个线程并不会去共享他，每个线程都有1份
  __thread int t_cachedTid = 0;		// 线程真实pid（tid）的缓存，
									// 是为了减少::syscall(SYS_gettid)系统调用的次数
									// 提高获取tid的效率
  __thread char t_tidString[32];	// 这是tid的字符串表示形式
  __thread const char* t_threadName = "unknown";//线程的名称
  const bool sameType = boost::is_same<int, pid_t>::value;//boost::is_same作用：若int和pid_t是相同类型的，则返回是true
  BOOST_STATIC_ASSERT(sameType);
}

namespace detail
{

pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}


void afterFork()
{
  muduo::CurrentThread::t_cachedTid = 0;//子进程中当前线程的tid=0
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid();//缓存一下tid
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

/*
（1）namespace detail中的pthread_atfork意义在于：
fork可能是在主线程中调用，也可能是在子线程中调用，
fork得到一个新进程，新进程中只有1个执行序列，只有1个线程(调用fork的线程被继承下来)

（2）实际上，对于编写多线程程序来说，我们最好不要调用fork，不要编写多线程多进程程序，
要么是用多进程，要么用多线程
*/

class ThreadNameInitializer
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main";//主线程的名称为main
    CurrentThread::tid();//缓存当前线程的tid到t_cachedTid中
    pthread_atfork(NULL, NULL, &afterFork);//若调用成功，则子进程则会调用afterFork
  }
};

ThreadNameInitializer init;//在detail名称空间中，先于main函数
}
}

using namespace muduo;

void CurrentThread::cacheTid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = detail::gettid();//detail名称空间的gettid()
    int n = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
    assert(n == 6); (void) n;//(void) n作用：防止release版本编译不通过，因为release版本编译的时候，assert(n == 6)这条语句
    //相当于没有，那么上一条语句的n相当于没用
  }
}

bool CurrentThread::isMainThread()
{
  return tid() == ::getpid();
}

AtomicInt32 Thread::numCreated_;

Thread::Thread(const ThreadFunc& func, const string& n)
  : started_(false),
    pthreadId_(0),
    tid_(0),
    func_(func),
    name_(n)
{
  numCreated_.increment();//原子操作，线程个数+1
}

Thread::~Thread()
{
  // no join
}

void Thread::start()
{
  assert(!started_);
  started_ = true;
  errno = pthread_create(&pthreadId_, NULL, &startThread, this);
  if (errno != 0)
  {
    //LOG_SYSFATAL << "Failed in pthread_create";
  }
}

int Thread::join()
{
  assert(started_);
  return pthread_join(pthreadId_, NULL);
}

//线程的入口函数，this指针传递进来
void* Thread::startThread(void* obj)
{
  Thread* thread = static_cast<Thread*>(obj);
  thread->runInThread();
  return NULL;
}

void Thread::runInThread()
{
  tid_ = CurrentThread::tid();//获取线程tid
  muduo::CurrentThread::t_threadName = name_.c_str();
  try
  {
    func_();//调用回调函数
    muduo::CurrentThread::t_threadName = "finished";
  }
  catch (const Exception& ex)
  {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
    throw; // rethrow
  }
}

