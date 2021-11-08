#!/bin/sh

set -x

SOURCE_DIR=`pwd`
BUILD_DIR=${BUILD_DIR:-./build}
BUILD_TYPE=${BUILD_TYPE:-release}
INSTALL_DIR=${INSTALL_DIR:-../${BUILD_TYPE}-install-cpp17}
CXX=${CXX:-/usr/bin/g++}

# CMAKE_BUILD_TYPE 设置build type
# CMAKE_INSTALL_PREFIX 类似于configure脚本的 -prefix
# CMAKE_EXPORT_COMPILE_COMMANDS=ON 生成makefile同时导出json文件

mkdir -p $BUILD_DIR/$BUILD_TYPE-cpp17 \
    && cd $BUILD_DIR/$BUILD_TYPE-cpp17 \
    && cmake \
        -DCMAKE_CXX_COMPILER=$CXX \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        $SOURCE_DIR \
    && make $*