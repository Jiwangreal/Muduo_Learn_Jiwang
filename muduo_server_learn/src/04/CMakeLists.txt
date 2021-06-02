cmake_minimum_required(VERSION 2.6)

project(pas CXX)

set(CXX_FLAGS -g -Wall)
set(CMAKE_CXX_COMPILER "g++")
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

##生成的可执行文件的路径在bin目录下
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

##一个文件生成可执行程序，多个文件生成可执行程序，都可以配置进来
add_executable(echosrv_poll echosrv_poll.cpp)
add_executable(echocli echocli.cpp)
