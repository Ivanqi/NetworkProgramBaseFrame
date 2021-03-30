#include <assert.h>
#include <iostream>
#include <time.h>  
#include <errno.h>
#include <sys/time.h> 
#include "networker/base/Logging.h"
#include "networker/base/CurrentThread.h"
#include "networker/base/Timestamp.h"
#include "networker/base/TimeZone.h"

namespace networker 
{
    __thread char t_errnobuf[512];
    __thread char t_time[64];
    __thread time_t t_lastSecond;

    const char* strerror_tl(int savedErrno)
    {
        return strerror_r(savedErrno, t_errnobuf, sizeof(t_errnobuf));
    }

    Logger::LogLevel initLogLevel()
    {
        if (::getenv("LOG_TRACE")) {
            return Logger::TRACE;
        } else if (::getenv("LOG_DEBUG")) {
            return Logger::DEBUG;
        } else {
            return Logger::INFO;
        }
    }

    Logger::LogLevel g_logLevel = initLogLevel();

    const char* LogLevelName[Logger::NUM_LOG_LEVELS] = {
        "TRACE ",
        "DEBUG ",
        "INFO  ",
        "WARN  ",
        "ERROR ",
        "FATAL ",
    };

    // 用于在编译时知道字符串长度的助手类
    class T
    {
        public:
            const char *str_;
            const unsigned len_;

            T(const char* str, unsigned len): str_(str), len_(len)
            {
                assert(strlen(str) == len_);
            }
    };

    inline LogStream& operator <<(LogStream&s, T v)
    {
        s.append(v.str_, v.len_);
        return s;
    }

    inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& v)
    {
        s.append(v.data_, v.size_);
        return s;
    }

    void defaultOutput(const char* msg, int len)
    {
        size_t n = fwrite(msg, 1, len, stdout);
        (void)n;
    }

    void defaultFlush()
    {
        fflush(stdout);
    }

    Logger::OutputFunc g_output = defaultOutput;
    Logger::FlushFunc g_flush = defaultFlush;
    TimeZone g_logTimeZone;
};

using namespace networker;

Logger::RecordBlock::RecordBlock(LogLevel level, int savedErrno, const SourceFile& file, int line) 
    : time_(Timestamp::now()), stream_(), 
    level_(level), line_(line), basename_(file)
{
  formatTime();

  CurrentThread::tid();
  stream_ << T(CurrentThread::tidString(), CurrentThread::tidStringLength()) << ' ';
  stream_ << T(LogLevelName[level], 6);
  
  if (savedErrno != 0) {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

// 记录当前时间
void Logger::RecordBlock::formatTime() 
{
    int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / Timestamp::kMicroSecondsPerSecond);
    int microseconds = static_cast<int> (microSecondsSinceEpoch % Timestamp::kMicroSecondsPerSecond);

    if (seconds != t_lastSecond) {
        t_lastSecond = seconds;
        struct tm tm_time;
        if (g_logTimeZone.valid()) {
            tm_time = g_logTimeZone.toLocalTime(seconds);
        } else {
            ::gmtime_r(&seconds, &tm_time);
        }

        int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
            tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
        
        assert(len == 17);
        (void)len;
    }

    if (g_logTimeZone.valid()) {
        Fmt us(".%06d ", microseconds);
        assert(us.length() == 8);
        stream_ << T(t_time, 17) << T(us.data(), 8);
    } else {
        Fmt us(".%06dZ ", microseconds);
        assert(us.length() == 9);
        stream_ << T(t_time, 17) << T(us.data(), 9);
    }
}

void Logger::RecordBlock::finish() 
{
    stream_ << " - " << basename_.data_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line): Redcord(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char* func): Redcord(level, 0, file, line)
{
    Redcord.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, LogLevel level): Redcord(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, bool toAbort): Redcord(toAbort ? FATAL: ERROR, errno, file, line)
{
}

Logger::~Logger() 
{
    Redcord.finish();
    const LogStream::Buffer& buf(stream().buffer());
    g_output(buf.data(), buf.length());
    if (Redcord.level_ == FATAL) {
        g_flush();
        abort();
    }
}

void Logger::setLogLevel(Logger::LogLevel level) 
{
    g_logLevel = level;
}

void Logger::setOutput(OutputFunc out)
{
    g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
    g_flush = flush;
}

void Logger::setTimeZone(const TimeZone& tz)
{
    g_logTimeZone = tz;
}