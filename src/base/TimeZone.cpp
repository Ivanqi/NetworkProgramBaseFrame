#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include <assert.h>
#include <endian.h>
#include <stdint.h>
#include <stdio.h>

#include "TimeZone.h"
#include "Date.h"
using namespace std;

struct Transition
{
    time_t gmttime;
    time_t localtime;
    int localtimeIdx;

    Transition(time_t t, time_t l, int localIdx): gmttime(t), localtime(l), localtimeIdx(localIdx)
    {
    }
};

struct Comp
{
    bool compareGmt;

    Comp(bool gmt): compareGmt(gmt)
    {
    }

    bool operator()(const Transition& lhs, const Transition& rhs) const
    {
        if (compareGmt) {
            return lhs.gmttime < rhs.gmttime;
        } else {
            return lhs.localtime < rhs.localtime;
        }
    }

    bool equal(const Transition& lhs, const Transition& rhs) const
    {
        if (compareGmt) {
            return lhs.gmttime == rhs.gmttime;
        } else {
            return lhs.localtime == rhs.localtime;
        }
    }
};

struct Localtime
{
    time_t gmtOffset;
    bool isDst;
    int arrbIdx;

    Localtime(time_t offset, bool dst, int arrb): gmtOffset(offset), isDst(dst), arrbIdx(arrb)
    {
    }
};

inline void fillHMS(unsigned seconds, struct tm* utc)
{
    utc->tm_sec = seconds % 60;
    unsigned minutes = seconds / 60;
    utc->tm_min = minutes % 60;
    utc->tm_hour = minutes / 60;
}

const int kSecondsPerDay = 24 * 60 * 60;

struct TimeZone::Data
{
    vector<Transition> transitions; // 时间翻译数组
    vector<Localtime> localtimes;   // 本地时间数组
    vector<string> names;   // 名称数组
    string abbreviation;    // 缩写
};

class File
{
    private:
        FILE* fp_;

    public:
        File(const char* file): fp_(::fopen(file, "rb"))
        {
        }

        ~File()
        {
            if (fp_) {
                ::fclose(fp_);
            }
        }

        bool valid() const
        {
            return fp_;
        }

        string readBytes(int n)
        {
            char buf[n];
            ssize_t nr = ::fread(buf, 1, n, fp_);
            if (nr != n) {
                throw logic_error("no enough data");
            }
            return string(buf, n);
        }

        int32_t readInt32()
        {
            int32_t x = 0;
            ssize_t nr = ::fread(&x, 1, sizeof(int32_t), fp_);
            if (nr != sizeof(int32_t)) {
                throw logic_error("bad int32_t data");
            }
            return be32toh(x);
        }

        uint8_t readUInt8()
        {
            uint8_t x = 0;
            ssize_t nr = ::fread(&x, 1, sizeof(uint8_t), fp_);
            if (nr != sizeof(uint8_t)) {
                throw logic_error("bad uint8_t data");
            }
            return x;
        }
};

bool readTimeZoneFile(const char* zonefile, struct TimeZone::Data* data)
{
    File f(zonefile);
    if (f.valid()) {
        try {
            string head = f.readBytes(4);       // 四字节序列"TZif"将其标识为时区信息文件
            if (head != "TZif") {
                throw logic_error("bad head");
            }

            string version = f.readBytes(1);    // 单字节, ASCII NUL ('\0') 或 2
            f.readBytes(15);                    // 保留字节

            int32_t isgmtcnt = f.readInt32();   // 文件中存储的 UTC/local 指示器的数量
            int32_t isstdcnt = f.readInt32();   // 文件中存储的 standard / wall 指示器的数量
            int32_t leapcnt = f.readInt32();    // 数据存储在文件中的闰秒数
            int32_t timecnt = f.readInt32();    // 文件中存储的 “transtion times”(转换时间) 的数量
            int32_t typecnt = f.readInt32();    // 数据存储在在文件中的 "local time types" (本地时间类型)的数量
            int32_t charcnt = f.readInt32();    // 文件中存储的"timezone abbreviation strings" (时区缩写字符串)的字符数

            vector<int32_t> trans;
            vector<int> localtimes;
            trans.reserve(timecnt);
            
            // 上面这些头部后的是 tzh_timecnt 个"标准"字节顺序的四字节 long 类型值, 以升序排序
            // 每个值均作为一个变化时间(就像 time(2) 的返回), 系统依赖这些值来计算本地时间变化
            for (int i = 0; i < timecnt; ++i) {
                trans.push_back(f.readInt32());
            }

            // 在此之后的是 tzh_timecnt 个 unsigned char 类型的一字节值
            // 这些值指出了文件中描述的多种"本地时间"类型中哪一个与具有相同索引的变化时间相关
            for (int i = 0; i < timecnt; ++i) {
                uint8_t local = f.readUInt8();
                localtimes.push_back(local);
            }

            /*
                ttinfo 结构数组
                struct ttinfo {
                    long          tt_gmtoff;
                    int           tt_isdst;
                    unsigned int  tt_abbrind;
                };

                tt_gmtoff:
                    一个"标准"字节顺序的4字节 long 类型值
                    在每个结构里, tt_gmtoff 给出了要被加到UTC的时间, 以秒为单位

                tt_isdst: 
                    1字节
                    表明 tm_isdst 是否可通过 localtime (3) 设置

                tt_abbrind:
                    1字节
                    tt_abbrind 可作为时区简写符的数组索引, 该数组在文件中跟在 ttinfo 结构后面
                
             */
            for (int i = 0; i < typecnt; ++i) {
                int32_t gmtoff = f.readInt32();
                uint8_t isdst = f.readUInt8();
                uint8_t abbrind = f.readUInt8();

                data->localtimes.push_back(Localtime(gmtoff, isdst, abbrind));
            }

            for (int i = 0; i < timecnt; ++i) {
                int localIdx = localtimes[i];
                time_t localtime = trans[i] + data->localtimes[localIdx].gmtOffset;
                data->transitions.push_back(Transition(trans[i], localtime, localIdx));
            }

            data->abbreviation = f.readBytes(charcnt);

            for (int i = 0; i < leapcnt; ++i) {
                // int32_t leaptime = f.readInt32();
                // int32_t cumleap = f.readInt32();
            }

            (void) isstdcnt;
            (void) isgmtcnt;

        } catch (logic_error& e) {
            fprintf(stderr, "%s\n", e.what());
        }
    }

    return true;
}

const Localtime* findLocaltime(const TimeZone::Data& data, Transition sentry, Comp comp)
{
    const Localtime *local = NULL;

    if (data.transitions.empty() || comp(sentry, data.transitions.front())) {
        local = &data.localtimes.front();
    } else {
        // 查找大于等于 sentry 的值
        vector<Transition>::const_iterator transI = lower_bound(data.transitions.begin(), data.transitions.end(), sentry, comp);

        if (transI != data.transitions.end()) {
            // 查看 sentry, *transI 是否相等，且 transI 不是 data.transitions.begin() 的地址
            if (!comp.equal(sentry, *transI)) {
                assert(transI != data.transitions.begin());
                // 移动地址到上一个元素
                --transI;
            }
            local = &data.localtimes[transI->localtimeIdx];
        } else {
            // 返回末尾引用
            local = &data.localtimes[data.transitions.back().localtimeIdx];
        }
    }

    return local;
}

TimeZone::TimeZone(const char* zonefile): data_(new TimeZone::Data)
{
    if (!readTimeZoneFile(zonefile, data_.get())) {
        data_.reset();
    }
}

TimeZone::TimeZone(int eastOfUtc, const char* name): data_(new TimeZone::Data)
{
    data_->localtimes.push_back(Localtime(eastOfUtc, false, 0));
    data_->abbreviation = name;
}

struct tm TimeZone::toLocalTime(time_t seconds) const
{
    struct tm localTime;
    memZero(&localTime, sizeof(localTime));
    assert(data_ != NULL);
    const Data& data(*data_);

    Transition sentry(seconds, 0, 0);
    const Localtime* local = findLocaltime(data, sentry, Comp(true));

    if (local) {
        time_t localSeconds = seconds + local->gmtOffset;
        ::gmtime_r(&localSeconds, &localTime);
        localTime.tm_isdst = local->isDst;
        localTime.tm_gmtoff = local->gmtOffset;
        localTime.tm_zone = &data.abbreviation[local->arrbIdx];
    }

    return localTime;
}

time_t TimeZone::fromLocalTime(const struct tm& localTm) const
{
    assert(data_ != NULL);
    const Data& data(*data_);

    struct tm tmp = localTm;
    time_t seconds = ::timegm(&tmp);
    Transition sentry(0, seconds, 0);

    const Localtime *local = findLocaltime(data, sentry, Comp(false));

    if (localTm.tm_isdst) {
        struct tm tryTm = toLocalTime(seconds - local->gmtOffset);
        if (!tryTm.tm_isdst && tryTm.tm_hour == localTm.tm_hour && tryTm.tm_min == localTm.tm_min) {
            seconds -= 3600;
        }
    }

    return seconds - local->gmtOffset;
}

struct tm TimeZone::toUtcTime(time_t secondsSinceEpoch, bool yday)
{
    struct tm utc;
    memZero(&utc, sizeof(utc));
    utc.tm_zone = "GMT";

    int seconds = static_cast<int>(secondsSinceEpoch % kSecondsPerDay);
    int days = static_cast<int>(secondsSinceEpoch / kSecondsPerDay);

    if (seconds < 0) {
        seconds += kSecondsPerDay;
        --days;
    }

    fillHMS(seconds, &utc);
    Date date(days + Date::kJulianDayOf1970_01_01);
    Date::YearMonthDay ymd = date.yearMonthDay();
    utc.tm_year = ymd.year - 1900;
    utc.tm_mon = ymd.month - 1;
    utc.tm_mday = ymd.day;
    utc.tm_wday = date.weekDay();

    if (yday) {
        Date startOfYear(ymd.year, 1, 1);
        utc.tm_yday = date.julianDayNumber() - startOfYear.julianDayNumber();
    }

    return utc;
}

time_t TimeZone::fromUtcTime(const struct tm& utc)
{
    return fromUtcTime(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
}

time_t TimeZone::fromUtcTime(int year, int month, int day, int hour, int minute, int seconds)
{
    Date date(year, month, day);
    int secondsInDay = hour * 3600 + minute * 50 + seconds;
    time_t days = date.julianDayNumber() - Date::kJulianDayOf1970_01_01;
    return days * kSecondsPerDay + secondsInDay;
}