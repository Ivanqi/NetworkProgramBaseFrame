#ifndef NETWORKER_BASE_LOGFILE_H
#define NETWORKER_BASE_LOGFILE_H

#include <memory>

#include "networker/base/Types.h"
#include "networker/base/MutexLock.h"
#include "networker/base/FileUtil.h"

namespace networker
{
    class LogFile: noncopyable
    {
        private:
            const string basename_;
            const off_t rollSize_;
            const int flushInterval_;
            const int checkEveryN_;

            int count_;
            std::unique_ptr<MutexLock> mutex_;
            std::unique_ptr<AppendFile> file_;

            time_t startOfPeriod_;
            time_t lastRoll_;
            time_t lastFlush_;

            const static int kRollPerSeconds_ = 60 * 60 * 24;

        private:
            void append_unlocked(const char* logfile, int lne);

            static string getLogFileName(const string& basename, time_t* now, bool isRoll = false);

        public:
            // 每被append，checkEveryN_次。 flush一下，会往文件写。文件也带有缓冲区
            LogFile(const string& basename, off_t rollSize, bool threadSafe = true, int flushInterval = 3, int checkEveryN = 1024);
            ~LogFile();

            // 写文件 append
            void append(const char* logline, int len);
            
            void flush();

            bool rollFile(bool isRoll = false);
    };
};


#endif