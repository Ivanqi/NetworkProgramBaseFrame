set(base_SRCS
    AsyncLogging.cpp
    CountDownLatch.cpp
    CurrentThread.cpp
    Date.cpp
    Exception.cpp
    FileUtil.cpp
    LogFile.cpp
    Logging.cpp
    LogStream.cpp
    ProcessInfo.cpp
    Thread.cpp
    Timestamp.cpp
    TimeZone.cpp
)

add_library(networker_base ${base_SRCS})
target_link_libraries(networker_base pthread)

# 添加install操作
install(TARGETS networker_base DESTINATION lib)

# GLOB 会产生一个由所有匹配globbing表达式的文件组成的列表，并将其保存到变量中
# 以下命令会把后面匹配到的所有文件交给GLOB子命令，由后者生成一个文件列表，并将列表赋值给HEADERS变量
file(GLOB HEADERS "*.h")
# 安装HEADERS, 把HEADERS内容添加到networker/base中
install(FILES ${HEADERS} DESTINATION include/networker/base)