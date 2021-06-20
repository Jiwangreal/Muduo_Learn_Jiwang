set(net_SRCS

  ##新增
  Channel.cc
  EventLoop.cc
  Poller.cc
  poller/DefaultPoller.cc
  poller/EPollPoller.cc
  poller/PollPoller.cc
  )

add_library(muduo_net ${net_SRCS})
target_link_libraries(muduo_net muduo_base)

install(TARGETS muduo_net DESTINATION lib)
set(HEADERS
  ##头文件：外部类，安装的时候提供下面的头文件
  Channel.h
  EventLoop.h
  # 其他头文件是内部类，就不需要安装了
  )
install(FILES ${HEADERS} DESTINATION include/muduo/net)

#add_subdirectory(http)
#add_subdirectory(inspect)
#
#if(NOT CMAKE_BUILD_NO_EXAMPLES)
#  add_subdirectory(tests)
#endif()
#
#if(PROTOBUF_FOUND)
#  add_subdirectory(protorpc)
#else()
#  add_subdirectory(protorpc EXCLUDE_FROM_ALL)
#endif()
