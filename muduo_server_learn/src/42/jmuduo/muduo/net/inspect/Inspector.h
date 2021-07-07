// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_INSPECT_INSPECTOR_H
#define MUDUO_NET_INSPECT_INSPECTOR_H

#include <muduo/base/Mutex.h>
#include <muduo/net/http/HttpRequest.h>
#include <muduo/net/http/HttpServer.h>

#include <map>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class ProcessInspector;

// A internal inspector of the running process, usually a singleton.
class Inspector : boost::noncopyable
{
 public:
  typedef std::vector<string> ArgList;
  typedef boost::function<string (HttpRequest::Method, const ArgList& args)> Callback;
  Inspector(EventLoop* loop,
            const InetAddress& httpAddr,
            const string& name);
  ~Inspector();

  // 如add("proc", "pid", ProcessInspector::pid, "print pid");
  // http://192.168.159.188:12345/proc/pid这个http请求就会相应的调用ProcessInspector::pid来处理
  // (模块名称，模块底下的命令，命令所对应的回调函数，帮助信息)
  // proc九四module，pid就是command，command需要回调cb来处理
  // add()会在commands_和helps_中添加一条记录
  void add(const string& module,
           const string& command,
           const Callback& cb,
           const string& help);

 private:
  typedef std::map<string, Callback> CommandList;//命令列表
  typedef std::map<string, string> HelpList;//帮助列表

  void start();
  void onRequest(const HttpRequest& req, HttpResponse* resp);

  // Inspector类是一个http服务器，所以包含了一个HttpServer对象
  HttpServer server_;
  // 暴露的接口，eg：/proc/status。由ProcessInspector实现，所以包含了一个processInspector_类对象
  boost::scoped_ptr<ProcessInspector> processInspector_;//processInspector_类对象由scoped_ptr来管理
                                                        //ProcessInspector类暴露的是proc模块module
  MutexLock mutex_;

  // <module， command，Callback>
  //module是key，command，Callback是value
  std::map<string, CommandList> commands_;//string指的是模块module

  // <module， command，help>
  std::map<string, HelpList> helps_;
};

}
}

#endif  // MUDUO_NET_INSPECT_INSPECTOR_H
