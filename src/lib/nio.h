/* nio interface
 */

#if !defined(_NIO_H_)
#define _NIO_H_

#include "libresb.h"

extern int nio_server_init(int);
extern int nio_epoll(struct epoll_event *, int);
extern int nio_epoll2(struct epoll_event *, int, int);
extern void nio_millisleep(int);
extern int nio_epoll_add(int, struct epoll_event *);
extern int nio_epoll_del(int);
extern void nio_unblock(int);
extern void nio_block(int);
extern int nio_client_connect(int, const char *);
extern ssize_t nio_readblock(int, void *, size_t);
extern ssize_t nio_writeblock(int, const void *, size_t);
extern int nio_client_init(const char *, int);

#endif
