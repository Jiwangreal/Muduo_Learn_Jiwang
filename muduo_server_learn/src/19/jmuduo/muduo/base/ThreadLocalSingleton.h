// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H
#define MUDUO_BASE_THREADLOCALSINGLETON_H

#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

namespace muduo
{

template<typename T>
class ThreadLocalSingleton : boost::noncopyable
{
 public:

  //返回单例对象，返回的是对象的引用
  static T& instance()
  {
    //不需要按照线程安全的方式去实现，因为每个线程都有一个指针
    if (!t_value_)
    {
      t_value_ = new T();
      deleter_.set(t_value_);//将指针设置进来
    }
    return *t_value_;
  }

  //返回对象的指针
  static T* pointer()
  {
    return t_value_;
  }

 private:

  static void destructor(void* obj)
  {
    assert(obj == t_value_);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];//不完全类型，编译器就会报错
    delete t_value_;
    t_value_ = 0;
  }

  //线程本地存储2：TSD，因为TSD还是用的LinuxC的API接口实现的，所以这里使用了嵌套类
  //嵌套类，目的仅仅使用用来回调destructor函数
  class Deleter
  {
   public:
    Deleter()
    {
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }

    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

  //线程本地存储机制1：__thread
  static __thread T* t_value_;//类型是 T*，加上__thread关键字，表示该指针，每一个线程都有一份
  static Deleter deleter_;//销毁指针所指向的对象，目的是调用destructor()时，指针所指向的对象能够自动被释放，而不要显式的释放
                          //deleter_对象被销毁时——>调用~Deleter()，同时也会调用&ThreadLocalSingleton::destructor——>
                          //destructor()函数的调用会销毁t_value_指针
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}
#endif
