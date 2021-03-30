#!/bin/sh
set -x

# install目录
install_dir="release-install-cpp17"
# include目录
include_dir="include"
# lib目录
lib_dir="lib"

# 系统include目录
system_include_dir="/usr/include"

# 系统lib目录
system_lib_dir="/usr/local/lib"

cd build/

if [[ ! -d "$install_dir" ]]; then
    echo "install 目录不存在";
    exit 1
fi

cd "$install_dir"

if [[ ! -d "$include_dir" ]]; then
    echo "include 目录不存在";
    exit 1
else
    if [ "`ls -A ${include_dir}`" = "" ]; then
        echo "${include_dir} 目录为空"
        exit 1
    fi

    cd "$include_dir"
    cp -rf ./* "$system_include_dir"
    cd ../
fi


if [[ ! -d "$lib_dir" ]]; then
    echo "include 目录不存在";
    exit 1
else
    if [ "`ls -A ${lib_dir}`" = "" ]; then
        echo "${lib_dir} 目录为空"
        exit 1
    fi

    cd "$lib_dir"
    cp -rf ./* "$system_lib_dir"
    cd ../
fi
