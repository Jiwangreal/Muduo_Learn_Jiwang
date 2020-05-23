set(base_SRCS
  Timestamp.cc
  )##相当于：base_SRCS=Timestamp.cc

add_library(muduo_base ${base_SRCS}) ##将base_SRCS的文件生成为muduo_base的库
target_link_libraries(muduo_base pthread rt)##需要链接pthread库和rt库

install(TARGETS muduo_base DESTINATION lib)
file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/muduo/base)

if(NOT CMAKE_BUILD_NO_EXAMPLES)
  add_subdirectory(tests)
endif()
