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

  // ��ǰ���������̣߳�ҵ���̣߳����ã���־����д����������
  void append(const char* logline, int len);

  // �����첽��־�߳�
  void start()
  {
    running_ = true;
    thread_.start(); // ��־�߳�����
    latch_.wait();//�ȴ�thread_.start();����
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

  // ������������̵߳��ã�������д����־�ļ���
  void threadFunc();

  //���������ͣ��̶���С��4M�Ļ�����
  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer;
  // ��ŵ�ptr_vector��Buffer��ָ����ptr_vector����
  /*
  ptr_vector��vector������
  vector�������ڲ���ָ�루��̬�ڴ棩��������
  ptr_vector�����ڲ���ָ�루��̬�ڴ棩�������ڣ���������ptr_vector�ڲ��Ķ�̬�ڴ�������ڣ����ǿ����Զ��ͷŵ�
  */
  typedef boost::ptr_vector<Buffer> BufferVector;

  typedef BufferVector::auto_type BufferPtr; // �����ΪBuffer������ָ�룬�ܹ���Buffer�������ڣ�
                                             // ������C++11�е�unique_ptr���߱��ƶ����壬���ܸ�ֵ��Ҳ���ܿ��������
                                             // ������unique_ptr����ָ��һ�����󣨲��ܿ�������ֻ��һ��ָ��ָ��ͬһ�����󣬲��ܽ��и��Ʋ���ֻ�ܽ����ƶ�������
                                            /*
                                            unique_ptr p1;
                                            unique_ptr p2;
                                            p1 = p2;//error
                                            
                                            p1 = std.move(p2);//���ԣ�p2��û��ָ����
                                            */

  const int flushInterval_; // ��ʱʱ�䣬��flushInterval_���ڣ�������ûд�����Խ��������е�����д���ļ���
  bool running_;

  string basename_;//��־�ļ���basename
  size_t rollSize_;//��־�ļ��Ĵ�С

  muduo::Thread thread_;
  muduo::CountDownLatch latch_;  // ���ڵȴ��߳�������ʵ�ʵ��߳�����֮���ٿ��Լ�����������
  
  muduo::MutexLock mutex_;
  muduo::Condition cond_;

  BufferPtr currentBuffer_; // ��ǰ������
  BufferPtr nextBuffer_;    // Ԥ��������
  BufferVector buffers_;    // ��д���ļ����������Ļ�������������ָ���б�
};

}
#endif  // MUDUO_BASE_ASYNCLOGGING_H
