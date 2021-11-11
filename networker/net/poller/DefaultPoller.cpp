#include "networker/net/Poller.h"
#include "networker/net/poller/PollPoller.h"
#include "networker/net/poller/EPollPoller.h"

#include <stdlib.h>
using namespace networker::net;

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
	if (::getenv("NETWORKER_USE_POLL")) {
    	return new PollPoller(loop);
  	} else {
    	return new EPollPoller(loop);
  	}
}