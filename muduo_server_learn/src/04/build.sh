#!/bin/sh

# ִ�е�������������
set -x

SOURCE_DIR=`pwd`
##���BUILD_DIRĿ¼��Ϊ�գ���ȡBUILD_DIR��ֵ�����Ϊ�գ���ȡbuild��ֵ
BUILD_DIR=${BUILD_DIR:-../build}

mkdir -p $BUILD_DIR \
  && cd $BUILD_DIR \
  && cmake $SOURCE_DIR \
  && make $*
##$*���ڴ��Σ������./build clean������ȵ���cmake��Ȼ�����make clean
