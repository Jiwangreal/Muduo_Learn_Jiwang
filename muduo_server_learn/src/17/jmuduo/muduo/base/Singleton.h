// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>
#include <stdlib.h> // atexit

namespace muduo
{

//线程安全类由模板实现
template<typename T>
class Singleton : boost::noncopyable
{
 public:
  //返回单例对象
  static T& instance()
  {
    pthread_once(&ponce_, &Singleton::init);//pthread_once保证init函数只被调用一次，保证对象就创建了一次，就是单例的
                                            //同样也保证了线程安全，当然也可以用锁的方式实现线程安全，但是效率没有pthread_once高
    return *value_;
  }

 private:
  Singleton();
  ~Singleton();

  //在内部创建对象
  static void init()
  {
    value_ = new T();
    ::atexit(destroy);//atexit注册一个函数来销毁，自动销毁（程序结束的时候，自动调用一个函数来销毁对象）
                      //同样也可以使用智能指针，利用智能指针来销毁
  }

  //在内部销毁对象
  static void destroy()
  {
    // T必须是一个完全类型
    /*
    incomplete_type含义：
    class A;前向声明，他就不是一个complete_type
    他仍然可以调用指针，也可以用delete删除它
    A* p;
    delete p;
    这样的话，编译不报错，只会报警告。这样会存在风险，因为类型class A是不完整的类型
    */
   //typedef定义了一个数组类型
   //char A[-1];数组的大小不能是-1，通过这样的手法让编译报错
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];//作用：在编译阶段发现错误
    delete value_;
  }

 private:
  static pthread_once_t ponce_;//该对象保证，一个函数只被执行一次
  static T*             value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

}
#endif

