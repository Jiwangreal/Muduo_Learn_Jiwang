#ifndef MUDUO_BASE_ASYNCLOGGING_H
#define MUDUO_BASE_ASYNCLOGGING_H

#include <muduo/base/BlockingQueue.h>
#include <muduo/base/BoundedBlockingQueue.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

#include <muduo/base/LogStream.h>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{

class AsyncLogging : boost::noncopyable
{
 public:

  AsyncLogging(const string& basename,
               size_t rollSize,
               int flushInterval = 3);

  ~AsyncLogging()
  {
    if (running_)
    {
      stop();
    }
  }

  // 供前端生产者线程（业务线程）调用（日志数据写到缓冲区）
  void append(const char* logline, int len);

  // 启动异步日志线程
  void start()
  {
    running_ = true;
    thread_.start(); // 日志线程启动
    latch_.wait();//等待thread_.start();启动
  }

  void stop()
  {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:

  // declare but not define, prevent compiler-synthesized functions
  AsyncLogging(const AsyncLogging&);  // ptr_container
  void operator=(const AsyncLogging&);  // ptr_container

  // 供后端消费者线程调用（将数据写到日志文件）
  void threadFunc();

  //缓冲区类型，固定大小的4M的缓冲区
  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer;
  // 存放到ptr_vector的Buffer的指针由ptr_vector负责
  /*
  ptr_vector与vector的区别：
  vector不负责内部的指针（动态内存）的生命期
  ptr_vector负责内部的指针（动态内存）的生命期，负责存放在ptr_vector内部的动态内存的生命期，它是可以自动释放的
  */
  typedef boost::ptr_vector<Buffer> BufferVector;

  typedef BufferVector::auto_type BufferPtr; // 可理解为Buffer的智能指针，能管理Buffer的生存期，
                                             // 类似于C++11中的unique_ptr，具备移动语义，不能赋值，也不能拷贝构造的
                                             // （两个unique_ptr不能指向一个对象（不能拷贝），只能一个指针指向同一个对象，不能进行复制操作只能进行移动操作）
                                            /*
                                            unique_ptr p1;
                                            unique_ptr p2;
                                            p1 = p2;//error
                                            
                                            p1 = std.move(p2);//可以，p2就没有指向了
                                            */

  const int flushInterval_; // 超时时间，在flushInterval_秒内，缓冲区没写满，仍将缓冲区中的数据写到文件中
  bool running_;

  string basename_;//日志文件的basename
  size_t rollSize_;//日志文件的大小

  muduo::Thread thread_;
  muduo::CountDownLatch latch_;  // 用于等待线程启动。实际的线程启动之后再可以继续其他操作
  
  muduo::MutexLock mutex_;
  muduo::Condition cond_;

  BufferPtr currentBuffer_; // 当前缓冲区
  BufferPtr nextBuffer_;    // 预备缓冲区
  BufferVector buffers_;    // 待写入文件的已填满的缓冲区，缓冲区指针列表
};

}
#endif  // MUDUO_BASE_ASYNCLOGGING_H
