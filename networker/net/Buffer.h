#ifndef NETWORKER_NET_BUFFER_H
#define NETWORKER_NET_BUFFER_H

#include "networker/base/StringPiece.h"
#include "networker/base/Types.h"
#include "networker/net/Endian.h"

#include <algorithm>
#include <vector>
#include <assert.h>
#include <string.h>

namespace networker
{
namespace net
{

    /// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
    ///
    /// @code
    /// +-------------------+------------------+------------------+
    /// | prependable bytes |  readable bytes  |  writable bytes  |
    /// |                   |     (CONTENT)    |                  |
    /// +-------------------+------------------+------------------+
    /// |                   |                  |                  |
    /// 0      <=      readerIndex   <=   writerIndex    <=     size
    /// @endcode

    class Buffer
    {
        private:
            std::vector<char> buffer_;
            size_t readerIndex_;
            size_t writerIndex_;
            static const char kCRLF[];

        public:
            static const size_t kCheapPrepend = 8;
            static const size_t kInitialSize = 1024;

            explicit Buffer(size_t initialSize = kInitialSize): buffer_(kCheapPrepend + kInitialSize), 
                readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend)
            {
                assert(readableBytes() == 0);
                assert(writableBytes() == initialSize);
                assert(prependableBytes() == kCheapPrepend);
            }

            void swap(Buffer& rhs)
            {
                buffer_.swap(rhs.buffer_);
                std::swap(readerIndex_, rhs.readerIndex_);
                std::swap(writerIndex_, rhs.writerIndex_);
            }

            size_t readableBytes() const
            {
                return writerIndex_ - readerIndex_;
            }

            size_t writableBytes() const
            {
                return buffer_.size() - writerIndex_;
            }

            size_t prependableBytes() const
            {
                return readerIndex_;
            }

            const char* peek() const
            {
                return begin() + readerIndex_;
            }

            const char* findCRLF() const
            {
                const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
                return crlf == beginWrite() ? NULL : crlf;
            }

            const char* findEOL() const
            {
                const void* eol = memchr(peek(), '\n', readableBytes());
                return static_cast<const char*>(eol);
            }

            const char* findEOL(const char* start) const
            {
                assert(peek() <= start);
                assert(start <= beginWrite());
                const void* eol = memchr(start, '\n', beginWrite() - start);
                return static_cast<const char*>(eol);
            }

            /**
             * retrieve返回void，防止字符串str（retrieve（readableBytes（）），readableBytes（））
             */
            void retrieve(size_t len)
            {
                assert(len <= readableBytes());
                if (len < readableBytes()) {
                    readerIndex_ += len;
                } else {
                    retrieveAll();
                }
            }

            void retrieveUntil(const char* end)
            {
                assert(peek() <= end);
                assert(end <= beginWrite());
                retrieve(end - peek());
            }

            void retrieveInt64()
            {
                retrieve(sizeof(int64_t));
            }

            void retrieveInt32()
            {
                retrieve(sizeof(int32_t));
            }

            void retrieveInt16()
            {
                retrieve(sizeof(int16_t));
            }

            void retrieveInt8()
            {
                retrieve(sizeof(int8_t));
            }

            void retrieveAll()
            {
                readerIndex_ = kCheapPrepend;
                writerIndex_ = kCheapPrepend;
            }

            string retrieveAllAsString()
            {
                return retrieveAsString(readableBytes());
            }

            string retrieveAsString(size_t len)
            {
                assert(len <= readableBytes());
                string result(peek(), len);
                retrieve(len);
                return result;
            }

            StringPiece toStringPiece() const
            {
                return StringPiece(peek(), static_cast<int>(readableBytes()));
            }

            void append(const StringPiece& str)
            {
                append(str.data(), str.size());
            }

            void append(const char* /*restrict*/ data, size_t len)
            {
                ensureWritableBytes(len);
                std::copy(data, data + len, beginWrite());
                hasWritten(len);
            }

            void append(const void* /*restrict*/ data, size_t len)
            {
                append(static_cast<const char*>(data), len);
            }

            void ensureWritableBytes(size_t len)
            {
                if (writableBytes() < len) {
                    makeSpace(len);
                }
                assert(writableBytes() >= len);
            }

            char* beginWrite()
            {
                return begin() + writerIndex_;
            }

            const char* beginWrite() const
            {
                return begin() + writerIndex_;
            }

            void hasWritten(size_t len)
            {
                assert(len <= writableBytes());
                writerIndex_ += len;
            }

            void unwrite(size_t len)
            {
                assert(len <= readableBytes());
                writerIndex_ -= len;
            }

            // 追加int64位的大端序列
            void appendInt64(int64_t x)
            {
                int64_t be64 = hostToNetwork64(x);
                append(&be64, sizeof(be64));
            }

            // 追加int32位的大端序列
            void appendInt32(int32_t x)
            {
                int32_t be32 = hostToNetwork32(x);
                append(&be32, sizeof(be32));
            }

            void appendInt16(int16_t x)
            {
                int16_t be16 = hostToNetwork16(x);
                append(&be16, sizeof(be16));
            }

            void appendInt8(int8_t x)
            {
                append(&x, sizeof(x));
            }

            // 读取int32位的主机序列
            int32_t readInt32()
            {
                int32_t result = peekInt32();
                retrieveInt32();
                return result;
            }

            int16_t readInt16()
            {
                int16_t result = peekInt16();
                retrieveInt16();
                return result;
            }

            int8_t readInt8()
            {
                int8_t result = peekInt8();
                retrieveInt8();
                return result;
            }

            // 看int64位的主机序列
            int64_t peekInt64() const
            {
                assert(readableBytes() >= sizeof(int64_t));
                int64_t be64 = 0;
                ::memcpy(&be64, peek(), sizeof(be64));
                return networkToHost64(be64);
            }

            // 看int32位的主机序列
            int32_t peekInt32() const
            {
                assert(readableBytes() >= sizeof(int32_t));
                int32_t be32 = 0;
                ::memcpy(&be32, peek(), sizeof(be32));
                return networkToHost32(be32);
            }

            int16_t peekInt16() const
            {
                assert(readableBytes() >= sizeof(int16_t));
                int16_t be16 = 0;
                ::memcpy(&be16, peek(), sizeof(be16));
                return networkToHost16(be16);
            }

            int8_t peekInt8() const
            {
                assert(readableBytes() >= sizeof(int8_t));
                int8_t x = *peek();
                return x;
            }

            // 预加int64位大端序列
            void prependInt64(int64_t x)
            {
                int64_t be64 = hostToNetwork64(x);
                prepend(&be64, sizeof(be64));
            }

            // 预加int32位的大端序列
            void prependInt32(int32_t x)
            {
                int32_t be32 = hostToNetwork32(x);
                prepend(&be32, sizeof(be32));
            }
            
            void prependInt16(int16_t x)
            {
                int16_t be16 = hostToNetwork16(x);
                prepend(&be16, sizeof(be16));
            }

            void prependInt8(int8_t x)
            {
                prepend(&x, sizeof(x));
            }

            /**
             * 前方添加
             * preenddata空间，让程序能以很低的代价在数据前面添加几个字节
             */
            void prepend(const void* /*restrict*/ data, size_t len)
            {
                assert(len <= prependableBytes());
                readerIndex_ -= len;
                const char *d = static_cast<const char*>(data);
                std::copy(d, d + len, begin() + readerIndex_);
            }

            void shrink(size_t reserve)
            {
                Buffer other;
                other.ensureWritableBytes(readableBytes() + reserve);
                other.append(toStringPiece());
                swap(other);
            }

            // 返回当前向量分配的存储空间大小
            size_t internalCapacity() const
            {
                return buffer_.capacity();
            }

            ssize_t readFd(int fd, int *savedErrno);

        private:
            char *begin()
            {
                return &*buffer_.begin();
            }

            const char* begin() const
            {
                return &*buffer_.begin();
            }

            void makeSpace(size_t len)
            {
                if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
                    // 重新设置存储内存
                    buffer_.resize(writerIndex_ + len);
                } else {
                    // 将可读数据移到前面，在缓冲区内留出空间
                    assert(kCheapPrepend < readerIndex_);
                    size_t readable = readableBytes();
                    // 把readerIndex_到writerIndex_的内存，移动到beign()处
                    std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);

                    readerIndex_ = kCheapPrepend;
                    writerIndex_ = readerIndex_ + readable;
                    assert(readable == readableBytes());
                }
            }
    };
};
};

#endif