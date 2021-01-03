#ifndef LOGGER_LOGGING_H
#define LOGGER_LOGGING_H

#include <pthread.h>
#include <string.h>
#include <string.h>
#include <stdio.h>

#include "LogStream.h"
#include "Timestamp.h"
#include "TimeZone.h"

class Logger
{       
    public:
        enum LogLevel {
            TRACE,
            DEBUG,
            INFO,
            WARN,
            ERROR,
            FATAL,
            NUM_LOG_LEVELS,
        };

        class SourceFile
        {
            public:
                const char* data_;
                int size_;

                template<int N>
                SourceFile(const char (&arr)[N]): data_(arr), size_(N - 1)
                {
                    const char *slash = strrchr(data_, '/');
                    if (slash) {
                        data_ = slash + 1;
                        size_ = static_cast<int>(data_ - arr);
                    }
                }

                explicit SourceFile(const char* filename): data_(filename)
                {
                    const char *slash = strrchr(filename, '/');
                    if (slash) {
                        data_ = slash + 1;
                    }
                    size_ = static_cast<int>(strlen(data_));
                }
        };

        Logger(SourceFile file, int line);
        Logger(SourceFile file, int line, LogLevel level);
        Logger(SourceFile file, int line, LogLevel level, const char* func);
        Logger(SourceFile file, int line, bool toAbort);

        ~Logger();
        LogStream& stream()
        {
            return Redcord.stream_;
        }

        static void setLogFileName(std::string fileName)
        {
            logFileName_ = fileName;
        }

        static std::string getLogFileName()
        {
            return logFileName_;
        }

        const char* getBuffer()
        {
            return Redcord.stream_.buffer().data();
        }

        static LogLevel logLevel();
        static void setLogLevel(LogLevel level);

        typedef void (*OutputFunc)(const char* msg, int len);
        typedef void (*FlushFunc)();

        static void setOutput(OutputFunc);
        static void setFlush(FlushFunc);
        static void setTimeZone(const TimeZone& tz);
        
     private:
        class RecordBlock
        {
            public:
                typedef Logger::LogLevel LogLevel;
                Timestamp time_;
                LogStream stream_;
                LogLevel level_;
                int line_;
                SourceFile basename_;

                RecordBlock(LogLevel level, int old_errno, const SourceFile& file, int line);
                void formatTime();
                void finish();
        };

        RecordBlock Redcord;
        static std::string logFileName_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
    return g_logLevel;
}

const char *strerror_tl(int savedErrno);

// 日志打印宏
#define LOG_TRACE if (Logger::logLevel() <= Logger::TRACE) \
  Logger(__FILE__, __LINE__, Logger::TRACE, __func__).stream()

#define LOG_DEBUG if (Logger::logLevel() <= Logger::DEBUG) \
  Logger(__FILE__, __LINE__, Logger::DEBUG, __func__).stream()

#define LOG_INFO if (Logger::logLevel() <= Logger::INFO) \
  Logger(__FILE__, __LINE__).stream()

#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).stream()

#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).stream()

#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).stream()

#define LOG_SYSERR Logger(__FILE__, __LINE__, false).stream()

#define LOG_SYSFATAL Logger(__FILE__, __LINE__, true).stream()


#endif