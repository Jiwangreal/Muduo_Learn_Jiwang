#include <muduo/base/AsyncLogging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>

using namespace muduo;

AsyncLogging::AsyncLogging(const string& basename,
                           size_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(boost::bind(&AsyncLogging::threadFunc, this), "Logging"),
    latch_(1),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),//当前缓冲区
    nextBuffer_(new Buffer),//预备缓冲区
    buffers_()
{
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16);
}

// 因为这里不涉及到写操作，对于前端业务线程来说，这里并发较大
void AsyncLogging::append(const char* logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len)
  {
    // 当前缓冲区未满，将数据追加到末尾
    currentBuffer_->append(logline, len);
  }
  else
  {
    // 当前缓冲区已满，将当前缓冲区添加到待写入文件的已填满的缓冲区列表
    buffers_.push_back(currentBuffer_.release());//currentBuffer_.release()：表示currentBuffer_就没有指向了

    // 将预备缓冲区设置为当前缓冲区
    if (nextBuffer_)
    {
      currentBuffer_ = boost::ptr_container::move(nextBuffer_); // 移动语义，将currentBuffer_指向nextBuffer_所指向的缓冲区
    }                                                           //且nextBuffer_不再有指向了
    else
    {
      // 这种情况，极少发生，前端写入速度太快，一下子把两块缓冲区都写完，
	  // 那么，只好分配一块新的缓冲区。
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);//将日志消息logline添加到当前缓冲区
    cond_.notify(); // 通知后端开始写入日志
  }
}

// 后端线程
void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  latch_.countDown();
  LogFile output(basename_, rollSize_, false);
  // 准备两块空闲缓冲区
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  while (running_)
  {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      muduo::MutexLockGuard lock(mutex_);
      if (buffers_.empty())  // unusual usage!（注意，这里是一个非常规用法，即使虚假唤醒，也写日志），最好用while，if不能解决虚假唤醒spurious wakeup问题
      {                     //虚假唤醒问题：这里可能并不是因为buffers_.empty()条件满足了而唤醒，可能是遇到了信号signal，waitForSeconds()运行
                            //时间比较久，可能会被信号打断，而不是因为buffers_不空
                            // 此外，在多处理器环境下（man pthread_cond_signal）,pthread_cond_signal可能唤醒多个等待条件变量中的线程，pthread_cond_signal
                            // 本来只能唤醒一个，所以虚假唤醒的线程都要用while()来解决
        // 条件变量在等待，等待不空
        cond_.waitForSeconds(flushInterval_); // 等待前端写满了一个或者多个buffer,或者一个超时时间到来，超时也写日志
      }

      buffers_.push_back(currentBuffer_.release()); // 将当前缓冲区移入buffers_
      currentBuffer_ = boost::ptr_container::move(newBuffer1); // 将空闲的newBuffer1置为当前缓冲区。移动语义，不涉及缓冲区的拷贝操作

      // 不涉及缓冲区的拷贝操作，仅是交换指针而已
      buffersToWrite.swap(buffers_); // buffers_与buffersToWrite交换，这样后面的代码可以在临界区之外安全地访问buffersToWrite
                                    //这样，前端业务线程可以与后端日志线程实现并发 
      // nextBuffer_为空了
      if (!nextBuffer_)
      {
        nextBuffer_ = boost::ptr_container::move(newBuffer2); // 确保前端始终有一个预备buffer可供调配，
                                                              // 减少前端临界区分配内存的概率，缩短前端临界区长度。
      }
    }

    // 真正写日志开始，buffersToWrite

    assert(!buffersToWrite.empty());

    // 消息堆积
    // 前端陷入死循环，拼命发送日志消息，超过后端的处理能力，这就是典型的生产速度
    // 超过消费速度问题，会造成数据在内存中堆积，严重时引发性能问题（可用内存不足，buffersToWrite就是内存）
    // 或程序崩溃（分配内存失败）
    if (buffersToWrite.size() > 25)//100M =25*4M，说明前端出现了死循环，拼命发送日志消息
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);
      output.append(buf, static_cast<int>(strlen(buf)));
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end()); // 丢掉多余日志，以腾出内存，仅保留两块缓冲区
    }

    for (size_t i = 0; i < buffersToWrite.size(); ++i)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffersToWrite[i].data(), buffersToWrite[i].length());//将缓冲区数据写入日志文件中
    }

    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2); // 仅保存两个buffer，用于newBuffer1与newBuffer2
    }

    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = buffersToWrite.pop_back();
      newBuffer1->reset();//将缓冲区的当前指针指向首部位置
    }

    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    // 释放掉多余的缓冲区
    buffersToWrite.clear();
    output.flush();
  }
  output.flush();
}

