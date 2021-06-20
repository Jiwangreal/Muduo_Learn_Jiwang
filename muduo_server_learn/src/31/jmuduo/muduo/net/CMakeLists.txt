set(net_SRCS
  Channel.cc
  EventLoop.cc
  EventLoopThread.cc
  # 新增
  InetAddress.cc
  Poller.cc
  poller/DefaultPoller.cc
  poller/EPollPoller.cc
  poller/PollPoller.cc
  # 新增
  Socket.cc
  SocketsOps.cc
  Timer.cc
  TimerQueue.cc
  )

add_library(muduo_net ${net_SRCS})
target_link_libraries(muduo_net muduo_base)

install(TARGETS muduo_net DESTINATION lib)
set(HEADERS
  Channel.h
  # 新增
  Endian.h
  EventLoop.h
  EventLoopThread.h
  # 新增，Socket.h不用放进来的，因为他是内部使用的
  InetAddress.h
  TimerId.h
  )
install(FILES ${HEADERS} DESTINATION include/muduo/net)

#add_subdirectory(http)
#add_subdirectory(inspect)
#
# 表示需要编译示例代码，需要将子文件夹tests添加进来
# 这样就会按照31\jmuduo\muduo\net\tests下的cmake进行编译
if(NOT CMAKE_BUILD_NO_EXAMPLES)
  add_subdirectory(tests)
endif()
#
#if(PROTOBUF_FOUND)
#  add_subdirectory(protorpc)
#else()
#  add_subdirectory(protorpc EXCLUDE_FROM_ALL)
#endif()
