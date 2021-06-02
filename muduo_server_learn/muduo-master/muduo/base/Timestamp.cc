// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Timestamp.h"

#include <sys/time.h>
#include <stdio.h>

//用于宏PRId64能用
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

using namespace muduo;

static_assert(sizeof(Timestamp) == sizeof(int64_t),
              "Timestamp is same size as int64_t");

string Timestamp::toString() const
{
  char buf[32] = {0};
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
  int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;

  /*
  int64_t用来表示64位整数，在32位系统中是long long int，在64位系统中是long int,所以打印int64_t的格式化方法是：
  printf(“%ld”, value);  // 64bit OS
  printf("%lld", value); // 32bit OS

  跨平台的做法：
  #define __STDC_FORMAT_MACROS
  #include <inttypes.h>
  #undef __STDC_FORMAT_MACROS 
  printf("%" PRId64 "\n", value);  
  */
  //PRId64用于跨平台的，from：<inttypes.h>，若是64bit的，就等于ld，若是32bit的，就等于lld
  snprintf(buf, sizeof(buf)-1, "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
  return buf;
}

string Timestamp::toFormattedString(bool showMicroseconds) const
{
  char buf[64] = {0};
  //求出距离1970.1.1的秒数
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
  struct tm tm_time;
  gmtime_r(&seconds, &tm_time);//_r表示线程，可以将秒数转换为tm_time结构体

  if (showMicroseconds)
  {
    int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);//转换成微妙数
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}

//获取当前时间
Timestamp Timestamp::now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);//gettimeofday(,时区)，NULL表示没有时区
  int64_t seconds = tv.tv_sec;//表示tv.tv_sec秒
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);//tv.tv_usec表示微妙
}

