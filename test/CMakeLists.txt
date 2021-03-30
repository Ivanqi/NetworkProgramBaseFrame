cmake_minimum_required(VERSION 2.6)

project(echo CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "debug")
endif()



message(STATUS "PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
message(STATUS "CMAKE_LIBRARY_PATH: " ${CMAKE_LIBRARY_PATH})
message(STATUS "CMAKE_INCLUDE_PATH: " ${CMAKE_INCLUDE_PATH})

if(NOT NETWORKER_PATH)
    set(NETWORKER_PATH "/usr")
    message(STATUS "NETWORKER_PATH: " ${NETWORKER_PATH})
endif()

if(NOT NETWORKER_LIB_PATH)
    set(NETWORKER_LIB_PATH ${NETWORKER_PATH} "/local")
    message(STATUS "NETWORKER_LIB_PATH: " ${NETWORKER_LIB_PATH})
endif()



set(CXX_FLAGS
    -g
    -Wall
    -std=c++17
    -Werror
    -rdynamic
)

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# find_path 用来在指定路径中搜索文件名
find_path(NetWorker_INCLUDE_DIR networker)
find_path(NetWorker_LIBRARY_DIR libmuduo_net.a HINTS "${NETWORKER_LIB_PATH}/lib")

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${NetWorker_LIBRARY_DIR})
message(STATUS "NetWorker_INCLUDE_DIR: " ${NetWorker_INCLUDE_DIR})
message(STATUS "NetWorker_LIBRARY_DIR: " ${NetWorker_LIBRARY_DIR})