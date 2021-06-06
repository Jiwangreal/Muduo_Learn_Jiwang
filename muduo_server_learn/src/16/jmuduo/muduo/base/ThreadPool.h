// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <deque>

namespace muduo
{

//���̳߳ص��߳���Ŀ�ǹ̶��ģ�Linux������ʵ����һ�������������̳߳�
class ThreadPool : boost::noncopyable
{
 public:
 //boost::function<void ()>��һ����������ִ������
  typedef boost::function<void ()> Task;

  explicit ThreadPool(const string& name = string());
  ~ThreadPool();

  //�����̳߳أ��������̸߳����̶�
  void start(int numThreads);
  //�ر��̳߳�
  void stop();

  //�����������̳߳ص���������������
  void run(const Task& f);

 private:
 //�̳߳��е��߳�Ҫִ�еĺ���
  void runInThread();
  //�̳߳��е��߳���Ҫ��ȡ����ȥִ������
  Task take();

  MutexLock mutex_;
  Condition cond_;//���������߳���ִ������
  string name_;//�߳�����
  boost::ptr_vector<muduo::Thread> threads_;//����߳�ָ��
  std::deque<Task> queue_;//ʹ��stl��deque��ʵ�֡��ڲ�������Ԫ��������Task
  bool running_;//��ʾ�̳߳��Ƿ�����
};

}

#endif
