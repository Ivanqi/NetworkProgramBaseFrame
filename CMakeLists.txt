cmake_minimum_required(VERSION 2.6)

project(networker C CXX)

enable_testing()

message("CMAKE_BUILD_BITS: " ${CMAKE_BUILD_BITS})
message("PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})
message("PROJECT_SOURCE_DIR:" ${PROJECT_SOURCE_DIR})
message("CMAKE_BUILD_TYPE:" ${CMAKE_BUILD_TYPE})
message("BUILD_TYPE:" ${BUILD_TYPE})
message("CMAKE_PROJECT_NAME:" ${CMAKE_PROJECT_NAME})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CXX_FLAGS
    -g
    -std=c++17
    -Werror
    -Wall
    -rdynamic # 用来通知链接器将所有符号添加到动态符号表中(目的是能够通过使用 dlopen 来实现向后跟踪)
)

if(CMAKE_BUILD_BITS EQUAL 32)
    list(APPEND CXX_FLAGS "-m32")
endif()

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

include_directories(${PROJECT_SOURCE_DIR})

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})


# add_subdirectory 指令
# 这个指令用于向当前工程添加存放源文件的子目录，并可以指定中间二进制和目标二进制存放的位置
add_subdirectory(networker/base)
add_subdirectory(networker/net)