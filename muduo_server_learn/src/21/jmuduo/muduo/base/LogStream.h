#ifndef MUDUO_BASE_LOGSTREAM_H
#define MUDUO_BASE_LOGSTREAM_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <assert.h>
#include <string.h> // memcpy
#ifndef MUDUO_STD_STRING
#include <string>
#endif
#include <boost/noncopyable.hpp>

namespace muduo
{

namespace detail
{

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000*1000;

// SIZE为非类型参数，而传递的是一个值
template<int SIZE>
class FixedBuffer : boost::noncopyable
{
 public:
  FixedBuffer()
    : cur_(data_)
  {
    setCookie(cookieStart);
  }

  ~FixedBuffer()
  {
    setCookie(cookieEnd);
  }

  void append(const char* /*restrict*/ buf, size_t len)
  {
    // FIXME: append partially，缓冲区空间不够了，能不能部分添加进去，还没有实现
    if (implicit_cast<size_t>(avail()) > len)
    {
      memcpy(cur_, buf, len);
      cur_ += len;//进行偏移
    }
  }

  const char* data() const { return data_; }//返回首地址
  int length() const { return static_cast<int>(cur_ - data_); }//当前缓冲区已经持有的容量

  // write to data_ directly
  char* current() { return cur_; }//返回指针
  int avail() const { return static_cast<int>(end() - cur_); }//当前可用的空间
  void add(size_t len) { cur_ += len; }

  void reset() { cur_ = data_; }//缓冲区重复利用，cur_指向首地址就可以
  void bzero() { ::bzero(data_, sizeof data_); }//清空缓冲区，相当于memset(date_,0,sizeof(data_))，后者更加通用而已

  // for used by GDB
  const char* debugString();//增加\0，将当前数据变成字符串
  void setCookie(void (*cookie)()) { cookie_ = cookie; }
  // for used by unit test
  string asString() const { return string(data_, length()); }//返回String类

 private:
  const char* end() const { return data_ + sizeof data_; }
  // Must be outline function for cookies.
  static void cookieStart();
  static void cookieEnd();

  void (*cookie_)();//目前没啥用
  char data_[SIZE];//data_就是缓冲区，该缓冲区并不是以二进制的方式存放的
  char* cur_;//当前指针
};

}

class LogStream : boost::noncopyable
{
  typedef LogStream self;
 public:
 //kSmallBuffer是一个常量，这是谷歌的编程规范
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;//实际输出的时候，首先输出到该缓冲区中

  //bool类型存放到缓冲区
  self& operator<<(bool v)
  {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  //都看成是整数放进去
  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  //指针的话，转换成16进制的地址存放进去
  self& operator<<(const void*);

  self& operator<<(float v)
  {
    *this << static_cast<double>(v);
    return *this;
  }
  self& operator<<(double);
  // self& operator<<(long double);

  self& operator<<(char v)
  {
    buffer_.append(&v, 1);
    return *this;
  }

  // self& operator<<(signed char);
  // self& operator<<(unsigned char);

  self& operator<<(const char* v)
  {
    buffer_.append(v, strlen(v));
    return *this;
  }

  //string是短字符优化的字符串
  /*
  这俩string接口一样，内部实现不太一样而已
  #ifdef MUDUO_STD_STRING
using std::string;
#else  // !MUDUO_STD_STRING
typedef __gnu_cxx::__sso_string string;
#endif
  */
  self& operator<<(const string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

#ifndef MUDUO_STD_STRING
  self& operator<<(const std::string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }
#endif

  //StringPiece也可以看成是一个字符串
  self& operator<<(const StringPiece& v)
  {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  void append(const char* data, int len) { buffer_.append(data, len); }
  const Buffer& buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template<typename T>
  void formatInteger(T);

  Buffer buffer_;//最终的目的就是调用插入运算符<<，把数据格式化到缓冲区中

  static const int kMaxNumericSize = 32;
};

class Fmt // : boost::noncopyable
{
 public:
 //成员模板
  template<typename T>
  Fmt(const char* fmt, T val);//将val按照fmt格式化到buf_

  const char* data() const { return buf_; }
  int length() const { return length_; }

 private:
  char buf_[32];
  int length_;
};

inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
  s.append(fmt.data(), fmt.length());
  return s;
}

}
#endif  // MUDUO_BASE_LOGSTREAM_H

