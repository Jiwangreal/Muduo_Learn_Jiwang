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

// SIZEΪ�����Ͳ����������ݵ���һ��ֵ
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
    // FIXME: append partially���������ռ䲻���ˣ��ܲ��ܲ�����ӽ�ȥ����û��ʵ��
    if (implicit_cast<size_t>(avail()) > len)
    {
      memcpy(cur_, buf, len);
      cur_ += len;//����ƫ��
    }
  }

  const char* data() const { return data_; }//�����׵�ַ
  int length() const { return static_cast<int>(cur_ - data_); }//��ǰ�������Ѿ����е�����

  // write to data_ directly
  char* current() { return cur_; }//����ָ��
  int avail() const { return static_cast<int>(end() - cur_); }//��ǰ���õĿռ�
  void add(size_t len) { cur_ += len; }

  void reset() { cur_ = data_; }//�������ظ����ã�cur_ָ���׵�ַ�Ϳ���
  void bzero() { ::bzero(data_, sizeof data_); }//��ջ��������൱��memset(date_,0,sizeof(data_))�����߸���ͨ�ö���

  // for used by GDB
  const char* debugString();//����\0������ǰ���ݱ���ַ���
  void setCookie(void (*cookie)()) { cookie_ = cookie; }
  // for used by unit test
  string asString() const { return string(data_, length()); }//����String��

 private:
  const char* end() const { return data_ + sizeof data_; }
  // Must be outline function for cookies.
  static void cookieStart();
  static void cookieEnd();

  void (*cookie_)();//Ŀǰûɶ��
  char data_[SIZE];//data_���ǻ��������û������������Զ����Ƶķ�ʽ��ŵ�
  char* cur_;//��ǰָ��
};

}

class LogStream : boost::noncopyable
{
  typedef LogStream self;
 public:
 //kSmallBuffer��һ�����������ǹȸ�ı�̹淶
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;//ʵ�������ʱ������������û�������

  //bool���ʹ�ŵ�������
  self& operator<<(bool v)
  {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  //�������������Ž�ȥ
  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  //ָ��Ļ���ת����16���Ƶĵ�ַ��Ž�ȥ
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

  //string�Ƕ��ַ��Ż����ַ���
  /*
  ����string�ӿ�һ�����ڲ�ʵ�ֲ�̫һ������
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

  //StringPieceҲ���Կ�����һ���ַ���
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

  Buffer buffer_;//���յ�Ŀ�ľ��ǵ��ò��������<<�������ݸ�ʽ������������

  static const int kMaxNumericSize = 32;
};

class Fmt // : boost::noncopyable
{
 public:
 //��Աģ��
  template<typename T>
  Fmt(const char* fmt, T val);//��val����fmt��ʽ����buf_

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

