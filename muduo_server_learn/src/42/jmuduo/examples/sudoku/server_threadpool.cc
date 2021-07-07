#include "sudoku.h"

#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/base/ThreadPool.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <utility>

#include <mcheck.h>
#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

class SudokuServer
{
 public:
  SudokuServer(EventLoop* loop, const InetAddress& listenAddr, int numThreads)
    : loop_(loop),
      server_(loop, listenAddr, "SudokuServer"),
      numThreads_(numThreads),
      startTime_(Timestamp::now())
  {
    server_.setConnectionCallback(
        boost::bind(&SudokuServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&SudokuServer::onMessage, this, _1, _2, _3));
	//server_.setThreadNum(4);
  }

  void start()
  {
    LOG_INFO << "starting " << numThreads_ << " threads.";
    threadPool_.start(numThreads_);
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_TRACE << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
  {
    LOG_DEBUG << conn->name();
    size_t len = buf->readableBytes();
    while (len >= kCells + 2)
    {
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
        string request(buf->peek(), crlf);
        buf->retrieveUntil(crlf + 2);
        len = buf->readableBytes();
        if (!processRequest(conn, request))
        {
          conn->send("Bad Request!\r\n");
          conn->shutdown();
          break;
        }
      }
      else if (len > 100) // id + ":" + kCells + "\r\n"
      {
        conn->send("Id too long!\r\n");
        conn->shutdown();
        break;
      }
      else
      {
        break;
      }
    }
  }

  bool processRequest(const TcpConnectionPtr& conn, const string& request)
  {
    string id;
    string puzzle;
    bool goodRequest = true;

    string::const_iterator colon = find(request.begin(), request.end(), ':');
    if (colon != request.end())
    {
      id.assign(request.begin(), colon);
      puzzle.assign(colon+1, request.end());
    }
    else
    {
      puzzle = request;
    }

    if (puzzle.size() == implicit_cast<size_t>(kCells))
    {
      // 计算任务让给计算线程池来处理，而不是IO线程来处理
      threadPool_.run(boost::bind(&solve, conn, puzzle, id));
    }
    else
    {
      goodRequest = false;
    }
    return goodRequest;
  }

  static void solve(const TcpConnectionPtr& conn,
                    const string& puzzle,
                    const string& id)
  {
    LOG_DEBUG << conn->name();
    string result = solveSudoku(puzzle);
    if (id.empty())
    {
      // send()是通过TcpConnection所属的IO线程来发送，而不是计算线程池所对应的线程来发送的
      conn->send(result+"\r\n");
    }
    else
    {
      conn->send(id+":"+result+"\r\n");
    }
  }

  EventLoop* loop_;
  TcpServer server_;
  ThreadPool threadPool_;//计算线程池
  int numThreads_;
  Timestamp startTime_;
};

/*
sudoku求解服务器，既是IO密集型，又是计算密集型的一个服务
（3）one loop per thread + thread pool （多个IO线程 + 计算线程池），这里只有一个IO线程+计算线程池

sudoku计算的代码放到计算线程池中处理的原因：
计算时间如果比较久，就会使得IO线程阻塞，IO线程很快就用尽了，就不能处理大量的并发连接了。
*/
int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  int numThreads = 0;
  if (argc > 1)
  {
    // 计算线程池的线程个数
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  InetAddress listenAddr(9981);
  SudokuServer server(&loop, listenAddr, numThreads);

  server.start();

  loop.loop();
}

