# NetWorker
## 程序安装
- 执行根目录下的 build.sh脚本, 对应生成的文件会存放在 build目录下
  - ./build.sh 类似于 make操作
  - ./build.sh install 类似于 make install 操作
- 把build目录中生成的头文件和静态链接库移动到系统目录下(Linux)
  - 默认的系统include目录: /usr/include
  - 默认的系统lib目录: /usr/local/lib
  - ./migration.sh

## 项目来源
- [Logger](https://github.com/Ivanqi/Logger)
- [EventLoop](https://github.com/Ivanqi/EventLoop)

## 线程
- 一般而言，多线程服务器中的线程可分为以下几类
  - IO线程(负责网络IO)
  - 计算线程(负责复杂计算)
  - 背景线程
- Log线程属于第三种，其它线程属于IO线程
- 除Log线程外，每个线程一个事件循环，遵循One loop per thread

## 并发模型
- [并发模型](./docs/model.png)
- MainReactor只有一个，负责响应client的连接请求，并建立连接，它使用一个NIO Selector
- 在建立连接后用Round Robin的方式分配给某个SubReactor,因为涉及到跨线程任务分配，需要加锁，这里的锁由某个特定线程中的loop创建，只会被该线程和主线程竞争
- SubReactor可以有一个或者多个，每个subReactor都会在一个独立线程中运行，并且维护一个独立的NIO Selector
- 当主线程把新连接分配给了某个SubReactor，该线程此时可能正阻塞在多路选择器(epoll)的等待中，怎么得知新连接的到来呢？这里使用了eventfd进行异步唤醒，线程会从epoll_wait中醒来，得到活跃事件，进行处理
  
## 核心结构
- Channel类
  - Channel是Reactor结构中的“事件”，它自始至终都属于一个EventLoop，负责一个文件描述符的IO事件，在Channel类中保存这IO事件的类型以及对应的回调函数，当IO事件发生时，最终会调用到Channel类中的回调函数
  - 因此，程序中所有带有读写时间的对象都会和一个Channel关联
- EventLoop
  - One loop per thread意味着每个线程只能有一个EventLoop对象，EventLoop即是时间循环，每次从poller里拿活跃事件，并给到Channel里分发处理
  - EventLoop中的loop函数会在最底层(Thread)中被真正调用，开始无限的循环，直到某一轮的检查到退出状态后从底层一层一层的退出

## Log
- Log的实现分为前端和后端，前端往后端写，后端往磁盘写
- Log前端是前面所述的IO线程，负责产生log，后端是Log线程，设计了多个缓冲区，负责收集前端产生的log，集中往磁盘写。这样，Log写到后端是没有障碍的，把慢的动作交给后端去做好了
- 后端主要是由多个缓冲区构成的，集满了或者时间到了就向文件写一次
  - 4个缓冲区分两组，每组的两个一个主要的，另一个防止第一个写满了没地方写，写满或者时间到了就和另外两个交换指针，然后把满的往文件里写
