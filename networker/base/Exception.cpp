#include "networker/base/Exception.h"
#include "networker/base/CurrentThread.h"

using namespace networker;

Exception::Exception(string msg) : message_(std::move(msg)), stack_(CurrentThread::stackTrace(/*demangle=*/false))
{
    
}