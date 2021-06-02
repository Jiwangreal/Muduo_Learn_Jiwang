#!/bin/sh

# 执行的所有命令都会跟踪
set -x

SOURCE_DIR=`pwd`
##如果BUILD_DIR目录不为空，则取BUILD_DIR的值；如果为空，则取build的值
BUILD_DIR=${BUILD_DIR:-../build}

mkdir -p $BUILD_DIR \
  && cd $BUILD_DIR \
  && cmake $SOURCE_DIR \
  && make $*
##$*用于传参，如果是./build clean，则会先调用cmake，然后调用make clean
