#ifndef NETWORKER_BASE_ASYNCLOGGING_H
#define NETWORKER_BASE_ASYNCLOGGING_H

#include <functional>
#include <string>
#include <vector>

#include "networker/base/CountDownLatch.h"
#include "networker/base/MutexLock.h"
#include "networker/base/Thread.h"
#include "networker/base/LogStream.h"

namespace networker
{
    class AsyncLogging: noncopyable
    {
        public:
            void threadFunc();
            typedef FixedBuffer<kLargeBuffer> Buffer;
            typedef std::vector<std::shared_ptr<Buffer>> BufferVector;
            typedef std::shared_ptr<Buffer> BufferPtr;
            const int flushInterval_;
            bool running_;
            std::string basename_;
            const off_t rollSize_;
            Thread thread_;
            MutexLock mutex_;
            Condition cond_;

            // 当前缓冲区
            BufferPtr currentBuffer_;

            // 预备缓冲区
            BufferPtr nextBuffer_;

            // 待写入文件已经填满的缓冲，供后端写入的Buffer
            BufferVector buffers_;
            CountDownLatch latch_;
        public:
            AsyncLogging(const std::string basename, off_t rollSize, int flushInterval = 3);

            ~AsyncLogging() 
            {
                if (running_) {
                    stop();
                }
            }

            void append(const char* logline, int len);

            void start()
            {
                running_ = true;
                thread_.start();
                latch_.wait();
            }

            void stop() {
                running_ = false;
                cond_.notify();
                thread_.join();
            }
    };

};


#endif