#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "networker/base/LogFile.h"
#include "networker/base/FileUtil.h"
#include "networker/base/ProcessInfo.h"

using namespace networker;
LogFile::LogFile(const string& basename, off_t rollSize, bool threadSafe, int flushInterval, int checkEveryN)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    checkEveryN_(checkEveryN),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
//   assert(basename.find('/') == string::npos);
  rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char* logline, int len)
{
    if (mutex_) {
        MutexLockGuard lock(*mutex_);
        append_unlocked(logline, len);
    } else {
        append_unlocked(logline, len);
    }    
}

void LogFile::flush()
{
    if (mutex_) {
        MutexLockGuard lock(*mutex_);
        file_->flush();
    } else {
        file_->flush();
    }
}

void LogFile::append_unlocked(const char* logline, int len) 
{
    file_->append(logline, len);
    // 写入的字节是否大于 要轮转的字节
    if (file_->writtenBytes() > rollSize_) {
        rollFile(true);
    } else {
        ++count_;
        // count_ 大于 checkEveryN_。需要检查，是否需要轮转日志或者情况文件缓存
        if (count_ >= checkEveryN_) {
            count_ = 0;
            time_t now = ::time(NULL);
            time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
            // thisPeriod_ != startOfPeriod_ 意味这轮转日志的时间已经不一样了
            if (thisPeriod_ != startOfPeriod_) {
                rollFile(true);
            } else if (now - lastFlush_ > flushInterval_) { // 清空文件缓存
                lastFlush_ = now;
                file_->flush();
            }
        }
    }
}

bool LogFile::rollFile(bool isRoll)
{
    time_t now = 0;
    string filename = getLogFileName(basename_, &now, isRoll);
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

    if (now > lastRoll_) {
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;
        file_.reset(new AppendFile(filename));
        return true;
    } else {
        return false;
    }
}

string LogFile::getLogFileName(const string& basename, time_t* now, bool isRoll)
{
    string filename;
    filename.reserve(basename.size() + 64);
    filename = basename;

    char timebuf[32];
    struct tm tm;

    *now = time(NULL);
    gmtime_r(now, &tm);

    strftime(timebuf, sizeof(timebuf), "%Y%m%d.", &tm);
    filename += timebuf;

    filename += ProcessInfo::hostname();
    filename += ".log";
    
    if (isRoll) {
        strftime(timebuf, sizeof timebuf, "-%Y%m%d_%H%M%S", &tm);
        string newname = filename + timebuf;
        newname += ".roll";
        rename(filename.c_str(), newname.c_str());
    }

    return filename;
}