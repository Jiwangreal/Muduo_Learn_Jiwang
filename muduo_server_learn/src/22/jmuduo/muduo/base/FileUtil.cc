// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/base/FileUtil.h>

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace muduo;

FileUtil::SmallFile::SmallFile(StringPiece filename)
  : fd_(::open(filename.data(), O_RDONLY | O_CLOEXEC)),//注意这里使用了系统调用而不是库函数fopen
    err_(0)
{
  buf_[0] = '\0';
  if (fd_ < 0)
  {
    err_ = errno;
  }
}

FileUtil::SmallFile::~SmallFile()
{
  if (fd_ >= 0)
  {
    ::close(fd_); // FIXME: check EINTR
  }
}

// return errno
template<typename String>
int FileUtil::SmallFile::readToString(int maxSize,
                                      String* content,
                                      int64_t* fileSize,
                                      int64_t* modifyTime,
                                      int64_t* createTime)
{
  BOOST_STATIC_ASSERT(sizeof(off_t) == 8);
  assert(content != NULL);
  int err = err_;
  if (fd_ >= 0)
  {
    content->clear();

    if (fileSize)
    {
      struct stat statbuf;
      //获取文件大小fstat
      if (::fstat(fd_, &statbuf) == 0)
      {
        if (S_ISREG(statbuf.st_mode))
        {
          *fileSize = statbuf.st_size;
          content->reserve(static_cast<int>(std::min(implicit_cast<int64_t>(maxSize), *fileSize)));
        }
        else if (S_ISDIR(statbuf.st_mode))
        {
          err = EISDIR;
        }
        if (modifyTime)
        {
          *modifyTime = statbuf.st_mtime;
        }
        if (createTime)
        {
          *createTime = statbuf.st_ctime;
        }
      }
      else
      {
        err = errno;
      }
    }

  //从文件中读取内容到字符串content
    while (content->size() < implicit_cast<size_t>(maxSize))
    {
      size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
      ssize_t n = ::read(fd_, buf_, toRead);
      if (n > 0)
      {
        content->append(buf_, n);
      }
      else
      {
        if (n < 0)
        {
          err = errno;
        }
        break;
      }
    }
  }
  return err;
}

int FileUtil::SmallFile::readToBuffer(int* size)
{
  int err = err_;
  if (fd_ >= 0)
  {
    //0表示偏移位置，即从头开始读
    ssize_t n = ::pread(fd_, buf_, sizeof(buf_)-1, 0);
    if (n >= 0)
    {
      if (size)
      {
        *size = static_cast<int>(n);
      }
      buf_[n] = '\0';
    }
    else
    {
      err = errno;
    }
  }
  return err;
}

//特化1
/*
特化的原因是：
  String类型可以取如下：
  #ifdef MUDUO_STD_STRING
using std::string;
#else  // !MUDUO_STD_STRING
typedef __gnu_cxx::__sso_string string
#endif;
*/
template int FileUtil::readFile(StringPiece filename,
                                int maxSize,
                                string* content,//特化
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::SmallFile::readToString(
    int maxSize,
    string* content,
    int64_t*, int64_t*, int64_t*);

#ifndef MUDUO_STD_STRING
//特化2
template int FileUtil::readFile(StringPiece filename,
                                int maxSize,
                                std::string* content,//特化
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::SmallFile::readToString(
    int maxSize,
    std::string* content,
    int64_t*, int64_t*, int64_t*);
#endif

