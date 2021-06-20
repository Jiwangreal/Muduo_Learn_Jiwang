// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#define __STDC_LIMIT_MACROS
#include <muduo/net/TimerQueue.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Timer.h>
#include <muduo/net/TimerId.h>

#include <boost/bind.hpp>

#include <sys/timerfd.h>

namespace muduo
{
namespace net
{
namespace detail
{

// 创建定时器
int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

// 计算超时时刻与当前时间的时间差
struct timespec howMuchTimeFromNow(Timestamp when)
{
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch();
  //微妙最多=100
  if (microseconds < 100)
  {
    microseconds = 100;
  }

  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);

  //纳秒    
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  
  //转换成struct timespec结构
  return ts;
}

// 清除定时器，避免一直触发
void readTimerfd(int timerfd, Timestamp now)
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

// 重置定时器的超时时间
//expiration是超时时刻
void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  bzero(&newValue, sizeof newValue);
  bzero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration);//将expiration转换为timespec
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);//到期之后会产生定时器事件
                                                                //则Poller会返回activeChannels，TimeerQueue的timerchannel则
                                                                //会返回回来，调用其handleEvent()，再回调handleRead()
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}
}
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
  //当定时器通道可读事件产生的时候，会回调handleRead()
  timerfdChannel_.setReadCallback(
      boost::bind(&TimerQueue::handleRead, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  //按照Channel的时序图，该定时器通道timerfdChannel_会加入到poll来关注
  //按照EventLoop的时序图，一旦timerfdChannel_可读事件产生的时候，就会回调handleRead()
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
  ::close(timerfd_);
  /*
  由于typedef std::set<Entry> TimerList;和 typedef std::set<ActiveTimer> ActiveTimerSet;是一样的，只释放一次就可以了
  */
  // do not remove channel, since we're in EventLoop::dtor();
  for (TimerList::iterator it = timers_.begin();
      it != timers_.end(); ++it)
  {
    delete it->second;
  }
}

// 增加一个定时器
TimerId TimerQueue::addTimer(const TimerCallback& cb,//定时器回调函数
                             Timestamp when,//超时时间
                             double interval)//间隔时间
{
  Timer* timer = new Timer(cb, when, interval);//构造定时器对象
                                               //when超时时间
                                               //每隔interval时间会产生一次定时器事件，回调cb函数
  /*
  //实现跨线程调用，即线程安全
  loop_->runInLoop(
      boost::bind(&TimerQueue::addTimerInLoop, this, timer));
	  */
  //直接调用addTimerInLoop(),则不是线程安全的，若调用addTimerInLoop不是EventLoop所属的I/O线程，则 loop_->assertInLoopThread();会失败
  //不能跨线程调用
  addTimerInLoop(timer);
  return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
  /*
  可以跨线程调用
  loop_->runInLoop(
      boost::bind(&TimerQueue::cancelInLoop, this, timerId));
	  */
   //不能跨线程调用
  cancelInLoop(timerId);
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
  // 断言处于I/O线程中
  loop_->assertInLoopThread();
  // 插入一个定时器，有可能会使得最早到期的定时器发生改变
  //插入的定时器可能比原有的定时器到期时间更早，此时earliestChanged=true
  bool earliestChanged = insert(timer);

  if (earliestChanged)
  {
    // 重置定时器的超时时刻(timerfd_settime)：最早的定时器先触发
    resetTimerfd(timerfd_, timer->expiration());//timer->expiration()是到期时间
  }
}

//取消一个定时器
void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequence_);//地址，序号
  // 查找该定时器
  ActiveTimerSet::iterator it = activeTimers_.find(timer);

  //找到了要取消的定时器
  if (it != activeTimers_.end())
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1); (void)n;
    delete it->first; // FIXME: no delete please,如果用了unique_ptr,这里就不需要手动删除了
    activeTimers_.erase(it);
  }
  else if (callingExpiredTimers_)//不在activeTimers_列表中，已经到期了
  {
    // 已经到期，并且正在调用回调函数的定时器
    cancelingTimers_.insert(timer);//便于reset()中已被取消的定时器就不需要再重启了
  }
  assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);		// 清除该事件，避免一直触发

  // 获取该时刻之前所有的定时器列表(即超时定时器列表)：即该时刻之前的所有定时器应该都超时了
  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;//处理到期的定时器的状态中
  cancelingTimers_.clear();//已经取消的定时器先clear

  // safe to callback outside critical section
  //一个个处理所有的超时的定时器
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    // 这里回调定时器处理函数
    it->second->run();
  }
  callingExpiredTimers_ = false;

  // 不是一次性定时器，需要重启
  reset(expired, now);
}

// rvo：Return Value Optimization返回值优化
//当一个定时器超时，还可能有更多的定时器超时，getExpired将所有的超时的定时器都返回回来 
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());

  std::vector<Entry> expired;

  // sentry是pair类型
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  // 返回第一个未到期的Timer的迭代器，即：第一个未到期的Timer的迭代器之前的定时器都是到期的
  // lower_bound的含义是返回第一个值>=sentry的元素的iterator
  // 即*end >= sentry，从而end->first > now
  /*
  sentry这个pair重载了>和<号运算符，比较的时候先比较now是否相等，还要比较reinterpret_cast<Timer*>(UINTPTR_MAX)地址是否相等
  而reinterpret_cast<Timer*>(UINTPTR_MAX)这个地址是最大的；
  若*end.now == sentry.nwo,则*end.Time地址一定要小于<reinterpret_cast<Timer*>(UINTPTR_MAX)地址，则不满足*end >= sentry，即
  end->first == now条件是永远不可能成立的，只可能end->first > now，
  所以使用了lower_bound，而不是upper_bound(没必要用，关键在于sentry是一个pair，其不仅要比较now，若相等，还要比较reinterpret_cast<Timer*>(UINTPTR_MAX)看谁更大)
  */
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);//所以这里的断言一定是now < end->first
                                                    //now < end->first是第一个大于now的时间，那么前面就是<=now的时间，就是到期的定时器


  // 将到期的定时器插入到expired中，不包含end，stl的算法都是[)区间
  std::copy(timers_.begin(), end, back_inserter(expired));//back_inserter插入迭代器，插入到expired中
  // 从timers_中移除到期的定时器
  timers_.erase(timers_.begin(), end);

  // 从activeTimers_中移除到期的定时器
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());

  //返回到期的定时器，return由于rvo优化的存在，这里并没有拷贝构造一个vector向量，所以不用担心性能问题
  return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
  Timestamp nextExpire;

  for (std::vector<Entry>::const_iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());
    // 如果是重复的定时器并且是未取消定时器（即该定时器没有被其他线程调用cancle()），则重启该定时器
    if (it->second->repeat()
        && cancelingTimers_.find(timer) == cancelingTimers_.end())//不在cancelingTimers_列表中
    {
      it->second->restart(now);//重新计算下一个超时时刻
      insert(it->second);//再次插入到定时器中
    }
    else
    {
      // 一次性定时器或者已被取消的定时器是不能重置的，因此删除该定时器
      // FIXME move to a free list
      delete it->second; // FIXME: no delete please
                        //见.h文件，Timer*是裸指针，使用unique_ptr，这样的话，就不需要手动调用delete
    }
  }

  if (!timers_.empty())
  {
    // 获取最早到期的定时器超时时间
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())
  {
    // 重置定时器的超时时刻(timerfd_settime)
    resetTimerfd(timerfd_, nextExpire);
  }
}

bool TimerQueue::insert(Timer* timer)
{
  //只能在I/O线程总调用
  loop_->assertInLoopThread();
 
  assert(timers_.size() == activeTimers_.size());
  // 最早到期时间是否改变
  bool earliestChanged = false;
  Timestamp when = timer->expiration();//到期时间
  TimerList::iterator it = timers_.begin();//将时间最早的定时器取出来，timers_用set实现，默认情况下：时间戳按照从小到大排列
  // 如果timers_为空或者when小于timers_中的最早到期时间
  if (it == timers_.end() || when < it->first)
  {
    earliestChanged = true;//最早到期的定时器发生改变
  }

   //timers_，activeTimers_存储的是同样的东西
  {
    // 插入到timers_中，按照到期时间来排序
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));
    
    //插入成功，则result.second肯定=true
    assert(result.second); (void)result;
  }
  {
    // 插入到activeTimers_中，按照定时器对象的地址来排序
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}

