#include <algorithm>
#include <limits>
#include <string.h>
#include <string>
#include <stdint.h>
#include <stdio.h>

#include "LogStream.h"

namespace networker
{
    const char digits[] = "9876543210123456789";
    const char* zero = digits + 9;

    template<typename T>
    size_t convert(char buf[], T value)
    {
        T i = value;
        char *p = buf;

        do {
            int lsd = static_cast<int>(i % 10);
            i /= 10;
            *p++ = zero[lsd];
        } while (i != 0);

        if (value < 0) {
            *p++ = '-';
        }
        *p = '\0';
        std::reverse(buf, p);

        return p - buf;
    }
};

using namespace networker;

template<typename T>
void LogStream::formatInteger(T v)
{
    // buffer容不下kMaxNumericSize个字符的话会被直接丢弃
    if (buffer_.avail() >= kMaxNumericSize) {
        size_t len = convert(buffer_.current(), v);
        buffer_.add(len);
    }
}

LogStream& LogStream::operator<<(short v) 
{
    *this << static_cast<int>(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned short v)
{
    *this << static_cast<unsigned int>(v);
    return *this;
}

LogStream& LogStream::operator<<(int v) 
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned int v) 
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(long v) 
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long v)
{
    formatInteger(v);
    return *this;
}


LogStream& LogStream::operator<<(long long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(double v)
{
    if (buffer_.avail() >= kMaxNumericSize) {
        int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v);
        buffer_.add(len);
    }
    return *this;
}

LogStream& LogStream::operator<<(long double v)
{
    if (buffer_.avail() >= kMaxNumericSize) {
        int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12Lg", v);
        buffer_.add(len);
    }
    return *this;
}

template<typename T>
Fmt::Fmt(const char* fmt, T val) 
{
    static_assert(std::is_arithmetic<T>::value == true, "Must be arithmetic type");

    length_ = snprintf(buf_, sizeof(buf_), fmt, val);

    assert(static_cast<size_t>(length_) < sizeof(buf_));
}

// Explicit instantiations

template Fmt::Fmt(const char* fmt, char);

template Fmt::Fmt(const char* fmt, short);
template Fmt::Fmt(const char* fmt, unsigned short);
template Fmt::Fmt(const char* fmt, int);
template Fmt::Fmt(const char* fmt, unsigned int);
template Fmt::Fmt(const char* fmt, long);
template Fmt::Fmt(const char* fmt, unsigned long);
template Fmt::Fmt(const char* fmt, long long);
template Fmt::Fmt(const char* fmt, unsigned long long);

template Fmt::Fmt(const char* fmt, float);
template Fmt::Fmt(const char* fmt, double);