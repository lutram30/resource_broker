/* nio interface
 */

#if !defined(_NIO_H_)
#define _NIO_H_

#include "libresb.h"

extern int nio_server_init(int);
extern int nio_epoll(struct epoll_event *, int);
extern void nio_millisleep(int);
extern int nio_epoll_add(int, struct epoll_event *);
extern void nio_unblock(int);
extern void nio_block(int);
extern int nio_client_connect(int, const char *);
extern int nio_readblock(int, char *, int);
extern int nio_writeblock(int, char *, int);

#endif