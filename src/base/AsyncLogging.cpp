#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <functional>

#include "AsyncLogging.h"
#include "LogFile.h"
#include "Timestamp.h"

AsyncLogging::AsyncLogging(std::string logFileName_, off_t rollSize, int flushInterval)
    :flushInterval_(flushInterval),
    running_(false),
    basename_(logFileName_),
    rollSize_(rollSize),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_(),
    latch_(1)
{
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::append(const char* logline, int len)
{
    MutexLockGuard lock(mutex_);
    if (currentBuffer_->avail() > len) {
        currentBuffer_->append(logline, len);
    } else {
        buffers_.push_back(currentBuffer_);
        currentBuffer_.reset();

        if (nextBuffer_) {
            currentBuffer_ = std::move(nextBuffer_);
        } else {
            currentBuffer_.reset(new Buffer);
        }

        currentBuffer_->append(logline, len);
        cond_.notify();
    }
}

void AsyncLogging::threadFunc()
{
    assert(running_ == true);
    latch_.countDown();

    // 直接IO的日志文件
    LogFile output(basename_, rollSize_, false);

    // 后端准备两个 Buffer, 预防临界区(超时，currentBuffer 写满)
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();

    // 用来和前端线程交换 Buffer
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);

    while (running_) {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());
        
        // 整段代码在临界区内，因此不会有任何race condition
        {
            MutexLockGuard lock(mutex_);
            // 暂时无日志，现在进行休眠等待
            if (buffers_.empty()) {
                cond_.waitForSeconds(flushInterval_);
            }

            // currentBuffer_ 数据加入到buffers_, 并清空数据
            buffers_.push_back(currentBuffer_);

            // 移动 newBuffer1 覆盖 currentBuffer_
            currentBuffer_ = std::move(newBuffer1);
            // buffersToWrite 交换 buffers_ 的数据
            buffersToWrite.swap(buffers_);

            // 如果 nextBuffer_ 不为空，移动newBuffer2 覆盖nextBuffer_
            if (!nextBuffer_) {
                // 这样前端始终有一个预备buffer可供调用。即保证前端有两个空缓冲可用
                // nextBuffer_可以减少前端临界区分配内存的概率，缩短前端临界区长度
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        assert(!buffersToWrite.empty());

        // buffersToWrite 超出25
        if (buffersToWrite.size() > 25) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Drooped log message at %s, %zd larger buffer\n",
                Timestamp::now().toFormattedString().c_str(), buffersToWrite.size() - 2);
            fputs(buf, stderr);
            output.append(buf, static_cast<int>(strlen(buf)));

            // 删除多余区域
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }

        // 将已经写满的 Buffer 写入到日志文件中，由LogFile 进行IO操作
        for (size_t i = 0; i < buffersToWrite.size(); ++i) {
            output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());
        }

        // 如果 buffersToWrite 大于 2，重置 buffersToWrite的长度为2.用于清空使用的两个缓存
        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }

        if (!newBuffer1) {
            assert(!buffersToWrite.empty());
            newBuffer1 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if (!newBuffer2) {
            assert(!buffersToWrite.empty());
            newBuffer2 = buffersToWrite.back();
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
        output.flush();
    }
    output.flush();
}