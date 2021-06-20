set(net_SRCS
  EventLoop.cc
  )

add_library(muduo_net ${net_SRCS})
# CurrentThread::tid()来自muduo_base，target_link_libraries(muduo_net muduo_base)
target_link_libraries(muduo_net muduo_base)

# make install安装的时候，会把头文件安装到相应目录
install(TARGETS muduo_net DESTINATION lib)
set(HEADERS
  EventLoop.h
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
