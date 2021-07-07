// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_HTTP_HTTPREQUEST_H
#define MUDUO_NET_HTTP_HTTPREQUEST_H

#include <muduo/base/copyable.h>
#include <muduo/base/Timestamp.h>
#include <muduo/base/Types.h>

#include <map>
#include <assert.h>
#include <stdio.h>

namespace muduo
{
namespace net
{

// 请求类
class HttpRequest : public muduo::copyable
{
 public:
//  请求的方法
  enum Method
  {
    // kGet, kPost, kHead, kPut, kDelete是有效的方法，实际上http协议不止这些方法
    kInvalid, kGet, kPost, kHead, kPut, kDelete
  };
  enum Version
  {
    kUnknown, kHttp10, kHttp11
  };

  HttpRequest()
    : method_(kInvalid),
      version_(kUnknown)
  {
  }

  void setVersion(Version v)
  {
    version_ = v;
  }

  Version getVersion() const
  { return version_; }

  bool setMethod(const char* start, const char* end)
  {
    assert(method_ == kInvalid);
    string m(start, end);//构造范围是[start,end)的字符串，迭代器用法
    if (m == "GET")
    {
      method_ = kGet;
    }
    else if (m == "POST")
    {
      method_ = kPost;
    }
    else if (m == "HEAD")
    {
      method_ = kHead;
    }
    else if (m == "PUT")
    {
      method_ = kPut;
    }
    else if (m == "DELETE")
    {
      method_ = kDelete;
    }
    else
    {
      method_ = kInvalid;
    }
    return method_ != kInvalid;
  }

  Method method() const
  { return method_; }

  // 请求方法转字符串
  const char* methodString() const
  {
    const char* result = "UNKNOWN";
    switch(method_)
    {
      case kGet:
        result = "GET";
        break;
      case kPost:
        result = "POST";
        break;
      case kHead:
        result = "HEAD";
        break;
      case kPut:
        result = "PUT";
        break;
      case kDelete:
        result = "DELETE";
        break;
      default:
        break;
    }
    return result;
  }

  // 设置路径
  void setPath(const char* start, const char* end)
  {
    path_.assign(start, end);
  }

  const string& path() const
  { return path_; }

  // 设置接收时间
  void setReceiveTime(Timestamp t)
  { receiveTime_ = t; }

  Timestamp receiveTime() const
  { return receiveTime_; }// 请求接收时间
  
  // 添加头部信息，
  // 结合一个一个典型的http请求，colon是一个冒号:
  void addHeader(const char* start, const char* colon, const char* end)
  {
    string field(start, colon);		// header域
    ++colon;
    // 去除左空格
    while (colon < end && isspace(*colon))
    {
      ++colon;
    }
    string value(colon, end);		// header值
    // 去除右空格
    while (!value.empty() && isspace(value[value.size()-1]))
    {
      value.resize(value.size()-1);//大小resize(value.size()-1)就相当于去除了右空格
    }
    headers_[field] = value;
  }

  // 根据头域返回它的值
  string getHeader(const string& field) const
  {
    string result;
    // 在map容器中查找并返回回来
    std::map<string, string>::const_iterator it = headers_.find(field);
    if (it != headers_.end())
    {
      result = it->second;
    }
    return result;
  }

  const std::map<string, string>& headers() const
  { return headers_; }

  // 交换。所有成员都得交换，version_估计作者忘记了
  void swap(HttpRequest& that)
  {
    std::swap(method_, that.method_);
    path_.swap(that.path_);
    receiveTime_.swap(that.receiveTime_);
    headers_.swap(that.headers_);
  }

 private:
  Method method_;		// 请求方法
  Version version_;		// 协议版本1.0/1.1
  string path_;			// 请求路径
  Timestamp receiveTime_;	// 请求接收时间
  std::map<string, string> headers_;	// header列表
};

}
}

#endif  // MUDUO_NET_HTTP_HTTPREQUEST_H
