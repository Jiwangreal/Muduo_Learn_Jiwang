// Taken from PCRE pcre_stringpiece.h
//
// Copyright (c) 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Sanjay Ghemawat
//
// A string like object that points into another piece of memory.
// Useful for providing an interface that allows clients to easily
// pass in either a "const char*" or a "string".
//
// Arghh!  I wish C++ literals were automatically of type "string".

// 用以实现高效的字符串传递
// void foo(const StringPiece& x);
// 这里既可以用const char*、也可以用std::string类型作为参数传递
// 并且不涉及内存拷贝
//目的是提升性能
/*
const char* s1;
const string s2;
将s1传递给x，可能会涉及到内部的字符串拷贝，效率不够高
void foo(const std::string& x);
*/

#ifndef MUDUO_BASE_STRINGPIECE_H
#define MUDUO_BASE_STRINGPIECE_H

#include <string.h>
#include <iosfwd>    // for ostream forward-declaration

#include <muduo/base/Types.h>
#ifndef MUDUO_STD_STRING
#include <string>
#endif

namespace muduo {

//StringPiece类可以把他当作是字符串，StringPiece类来自谷歌
class StringPiece {
 private:
  const char*   ptr_;
  int           length_;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is
  // expected.
  StringPiece()
    : ptr_(NULL), length_(0) { }

  //将char*传递进来
  StringPiece(const char* str)
    : ptr_(str), length_(static_cast<int>(strlen(ptr_))) { }
  StringPiece(const unsigned char* str)
    : ptr_(reinterpret_cast<const char*>(str)),
      length_(static_cast<int>(strlen(ptr_))) { }

  //将字符串对象传递进来
  //ptr_指向字符串里面的数据str.data()，这样string对象作为参数传递进来，就不涉及到内存拷贝，仅是指针的操作
  //这里的string是短字符串优化的string
  StringPiece(const string& str)
    : ptr_(str.data()), length_(static_cast<int>(str.size())) { }
#ifndef MUDUO_STD_STRING
  //使用stl中的string
  StringPiece(const std::string& str)
    : ptr_(str.data()), length_(static_cast<int>(str.size())) { }
#endif
  //将字符串某几个长度传递进来
  StringPiece(const char* offset, int len)
    : ptr_(offset), length_(len) { }

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.  Use "as_string().c_str()" if you really need to do
  // this.  Or better yet, change your routine so it does not rely on NUL
  // termination.
  //返回指针
  const char* data() const { return ptr_; }
  //返回长度
  int size() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() { ptr_ = NULL; length_ = 0; }
  void set(const char* buffer, int len) { ptr_ = buffer; length_ = len; }
  void set(const char* str) {
    ptr_ = str;
    length_ = static_cast<int>(strlen(str));
  }
  void set(const void* buffer, int len) {
    ptr_ = reinterpret_cast<const char*>(buffer);
    length_ = len;
  }

  //下标运算符
  char operator[](int i) const { return ptr_[i]; }

  //去除前缀
  void remove_prefix(int n) {
    ptr_ += n;
    length_ -= n;
  }

  //去除后缀
  void remove_suffix(int n) {
    length_ -= n;
  }

  //==运算符借助内存比较
  bool operator==(const StringPiece& x) const {
    return ((length_ == x.length_) &&
            (memcmp(ptr_, x.ptr_, length_) == 0));
  }
  bool operator!=(const StringPiece& x) const {
    return !(*this == x);
  }

//cmp是比较运算符，auxcmp是辅助比较运算符
/*
length_ < x.length_ ? length_ : x.length_取小的
eg:STRINGPIECE_BINARY_PREDICATE(<,  <);
<, <
第一个是主的，第二个是辅助的
字符串abcd 字符串abcdefg，这俩字符串比较，abcd应该小于abcdefg，应该返回为真
经过memcmp比较之后，memcpy只是比较了前4个字符，返回的结果r=0，
接着执行return里面的：取||后，应该返回为真


eg：
<, <
字符串abcd 字符串abcdefg，abcd应该小于abcdefg，返回值应该为假
memcpy之后，返回值r>0；
return取||之后，返回为假
*/
#define STRINGPIECE_BINARY_PREDICATE(cmp,auxcmp)                             \
  bool operator cmp (const StringPiece& x) const {                           \
    int r = memcmp(ptr_, x.ptr_, length_ < x.length_ ? length_ : x.length_); \
    return ((r auxcmp 0) || ((r == 0) && (length_ cmp x.length_)));          \
  }

  //4个宏实现了4个运算符
  STRINGPIECE_BINARY_PREDICATE(<,  <);
  STRINGPIECE_BINARY_PREDICATE(<=, <);
  STRINGPIECE_BINARY_PREDICATE(>=, >);
  STRINGPIECE_BINARY_PREDICATE(>,  >);
#undef STRINGPIECE_BINARY_PREDICATE

  //2个字符串比较
  int compare(const StringPiece& x) const {
    int r = memcmp(ptr_, x.ptr_, length_ < x.length_ ? length_ : x.length_);
    if (r == 0) {
      if (length_ < x.length_) r = -1;
      else if (length_ > x.length_) r = +1;
    }
    return r;
  }

  string as_string() const {
    return string(data(), size());
  }

  void CopyToString(string* target) const {
    target->assign(ptr_, length_);
  }

#ifndef MUDUO_STD_STRING
  void CopyToStdString(std::string* target) const {
    target->assign(ptr_, length_);
  }
#endif

  // Does "this" start with "x"
  bool starts_with(const StringPiece& x) const {
    return ((length_ >= x.length_) && (memcmp(ptr_, x.ptr_, x.length_) == 0));
  }
};

}   // namespace muduo

// ------------------------------------------------------------------
// Functions used to create STL containers that use StringPiece
//  Remember that a StringPiece's lifetime had better be less than
//  that of the underlying string or char*.  If it is not, then you
//  cannot safely store a StringPiece into an STL container
// ------------------------------------------------------------------

/*
C++ template traits机制，参考《C++ template》这本书
在STL中为了提供通用的操作而又不损失效率，我们用到了一种特殊的技巧，叫traits编程技巧。
具体的来说，traits就是通过定义一些结构体或类，并利用模板特化和偏特化的能力，
给类型赋予一些特性，这些特性根据类型的 不同而异。
在程序设计中可以使用这些traits来判断一个类型的一些特性，引发C++的函数重载机制，
实现同一种操作因类型不同而异的效果。

template <typename T>
{
  对所有类型的操作都是通用的，但是可能对某些类型的操作会有损失，所以某些类型
  需要对他进行特化，采用特殊的实现。所以可以给类型T加上一些特征，这里面的代码根据这些类型特征来提供不同的实现。
  也就说这里面还有一些多态，
  。。。
  。。。
}
*/

// 这里对__type_traits进行特化，给StringPiece一些特性
#ifdef HAVE_TYPE_TRAITS
// This makes vector<StringPiece> really fast for some STL implementations，将StringPiece放到向量vector中
//若StringPiece类有以下的特征，可能运行起来更快

//__type_traits类型特性，要求STL要有HAVE_TYPE_TRAITS
//__type_traits修饰的类叫类型特性类，叫traits类
//这里是模板，实例化muduo::StringPiece类，就叫特化
//给StringPiece类施加了一些特性，它具有下面的特性
template<> struct __type_traits<muduo::StringPiece> {
  typedef __true_type    has_trivial_default_constructor;
  typedef __true_type    has_trivial_copy_constructor;
  typedef __true_type    has_trivial_assignment_operator;
  typedef __true_type    has_trivial_destructor;
  typedef __true_type    is_POD_type;
};
#endif

// allow StringPiece to be logged
std::ostream& operator<<(std::ostream& o, const muduo::StringPiece& piece);

#endif  // MUDUO_BASE_STRINGPIECE_H
