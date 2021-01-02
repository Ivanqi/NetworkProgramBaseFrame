#ifndef EVENT_TIMERID_H
#define EVENT_TIMERID_H

class Timer;

class TimerId
{
    private:
        Timer* timer_;
        int64_t sequence_;

    public:
        TimerId(): timer_(NULL), sequence_(0)
        {

        }

        TimerId(Timer *timer, int64_t seq): timer_(timer), sequence_(seq)
        {

        }

        friend class TimerQueue;
};

#endif