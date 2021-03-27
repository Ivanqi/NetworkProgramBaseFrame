include(CheckFunctionExists)

check_function_exists(accept4 HAVE_ACCEPT4)
if(NOT HAVE_ACCEPT4)
    set_source_files_properties(SocketsOps.cpp PROPERTIES COMPILE_FLAGS "-DNO_ACCEPT4")
endif()

set(net_SRCS
    Acceptor.cpp
    Buffer.cpp
    Channel.cpp
    Connector.cpp
    EventLoop.cpp
    EventLoopThread.cpp
    EventLoopThreadPool.cpp
    InetAddress.cpp
    Poller.cpp
    poller/DefaultPoller.cpp
    poller/EPollPoller.cpp
    poller/PollPoller.cpp
    Socket.cpp
    SocketsOps.cpp
    TcpClient.cpp
    TcpConnection.cpp
    TcpServer.cpp
    Timer.cpp
    TimerQueue.cpp
)

add_library(networker_net ${net_SRCS})
target_link_libraries(networker_net networker_base)

# 添加install操作
install(TARGETS networker_net DESTINATION lib)

set(HEADERS
    Buffer.h
    Callbacks.h
    Channel.h
    Endian.h
    EventLoop.h
    EventLoopThread.h
    EventLoopThreadPool.h
    InetAddress.h
    TcpClient.h
    TcpConnection.h
    TcpServer.h
    TimerId.h
)

install(FILES ${HEADERS} DESTINATION include/networker/net)
