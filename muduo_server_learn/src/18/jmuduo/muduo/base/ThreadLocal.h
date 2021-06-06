// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

//ThreadLocal类也是不能拷贝的
//这个类以模板方式封装， 则可以把非POD类型的类放进来
template<typename T>
class ThreadLocal : boost::noncopyable
{
 public:
  ThreadLocal()
  {
    //创建key
    pthread_key_create(&pkey_, &ThreadLocal::destructor);//用回调函数destructor来销毁实际的数据
  }

  ~ThreadLocal()
  {
    //销毁key，并不销毁实际数据！
    pthread_key_delete(pkey_);
  }

  //实际的特定数据
  T& value()
  {
    //获取线程特定数据
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));//转换成T*
    //返回的指针若是空的
    if (!perThreadValue) 
    {
      T* newObj = new T();
      pthread_setspecific(pkey_, newObj);//设定数据
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:

  static void destructor(void *x)
  {
    T* obj = static_cast<T*>(x);
    //检测是一个完全类型
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete obj;
  }

 private:
  pthread_key_t pkey_;//key的类型
};

}
#endif
