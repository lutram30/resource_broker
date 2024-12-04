
#if !defined(_RBSRV_H_)
#define _RBSRV_H

#include "../lib/libresb.h"
#include "../lib/nio.h"

#define MAX_SRV_EVENTS 32

extern struct rb_server *srv;

extern int register_with_broker(struct rb_daemon_id *);
extern int process_broker_request(int);
extern int handle_broker_events(int, struct epoll_event *);

#endif
