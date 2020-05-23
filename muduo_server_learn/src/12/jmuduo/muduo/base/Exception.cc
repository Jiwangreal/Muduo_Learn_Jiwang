// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Exception.h>

#include <cxxabi.h>//demangle需要的头文件
#include <execinfo.h>
#include <stdlib.h>
#include <stdio.h>//demangle需要的头文件

using namespace muduo;

Exception::Exception(const char* msg)
  : message_(msg)
{
  fillStackTrace();
}

Exception::Exception(const string& msg)
  : message_(msg)
{
  fillStackTrace();
}

Exception::~Exception() throw ()
{
}

const char* Exception::what() const throw()
{
  return message_.c_str();
}

const char* Exception::stackTrace() const throw()
{
  return stack_.c_str();
}

//构造函数中，已经将栈回溯信息给填充了，下面照抄man backtrace的eg
//登记栈回溯信息，保存在变量stack_中
void Exception::fillStackTrace()
{
  const int len = 200;
  void* buffer[len];//指针数组

  //backtrace:栈回溯，保存各个栈帧的地址
  int nptrs = ::backtrace(buffer, len);//将栈帧的函数地址保存在指针buffer所指向的数组中

  //backtrace_symbols，根据地址，转成相应的函数符号
  char** strings = ::backtrace_symbols(buffer, nptrs);//将地址转换成函数名称
  if (strings)
  {
    for (int i = 0; i < nptrs; ++i)
    {
      // TODO demangle funcion name with abi::__cxa_demangle用于将函数名字还原
      //stack_.append(strings[i]);//C++的函数名字，会做名字改编，改编成如下图所示
	  stack_.append(demangle(strings[i]));//名字不改编的写法
      stack_.push_back('\n');
    }
    free(strings);
  }
}
//demangle名字不改编的实现
string Exception::demangle(const char* symbol)
{
  size_t size;
  int status;
  char temp[128];
  char* demangled;
  //first, try to demangle a c++ name
  if (1 == sscanf(symbol, "%*[^(]%*[^_]%127[^)+]", temp)) {
    if (NULL != (demangled = abi::__cxa_demangle(temp, NULL, &size, &status))) {
      string result(demangled);
      free(demangled);
      return result;
    }
  }
  //if that didn't work, try to get a regular c symbol
  if (1 == sscanf(symbol, "%127s", temp)) {
    return temp;
  }
 
  //if all else fails, just return the symbol
  return symbol;
}
