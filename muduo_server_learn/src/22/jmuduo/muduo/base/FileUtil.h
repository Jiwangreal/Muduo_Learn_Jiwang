// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_BASE_FILEUTIL_H
#define MUDUO_BASE_FILEUTIL_H

#include <muduo/base/Types.h>
#include <muduo/base/StringPiece.h>
#include <boost/noncopyable.hpp>

namespace muduo
{

namespace FileUtil
{

  //С�ļ���ȡ
  class SmallFile : boost::noncopyable
  {
   public:
    SmallFile(StringPiece filename);
    ~SmallFile();

    // return errno
    //��ȡ��contentû������
    template<typename String>
    int readToString(int maxSize,
                     String* content,
                     int64_t* fileSize,
                     int64_t* modifyTime,
                     int64_t* createTime);

    // return errno
    int readToBuffer(int* size);//��ȡ��������������

    const char* buffer() const { return buf_; }

    static const int kBufferSize = 65536;

   private:
    int fd_;
    int err_;
    char buf_[kBufferSize];
  };


  //��filename�����浽content�ַ�����
  /*
  String���Ϳ���ȡ���£�
  #ifdef MUDUO_STD_STRING
using std::string;
#else  // !MUDUO_STD_STRING
typedef __gnu_cxx::__sso_string string
#endif;
  */
  // read the file content, returns errno if error happens.
  template<typename String>
  int readFile(StringPiece filename,
               int maxSize,
               String* content,
               int64_t* fileSize = NULL,//�ļ���С
               int64_t* modifyTime = NULL,//�ļ��޸�ʱ��
               int64_t* createTime = NULL)//�ļ�����ʱ��
  {
    SmallFile file(filename);
    return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
  }

}

}

#endif  // MUDUO_BASE_FILEUTIL_H

