set(base_SRCS
  Condition.cc
  CountDownLatch.cc
  Exception.cc
  # 新增
  FileUtil.cc
  # 新增
  LogFile.cc
  Logging.cc
  LogStream.cc
  # 新增
  ProcessInfo.cc
  Thread.cc
  ThreadPool.cc
  Timestamp.cc
  )

add_library(muduo_base ${base_SRCS})
target_link_libraries(muduo_base pthread rt)

install(TARGETS muduo_base DESTINATION lib)
file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/muduo/base)

if(NOT CMAKE_BUILD_NO_EXAMPLES)
  add_subdirectory(tests)
endif()
