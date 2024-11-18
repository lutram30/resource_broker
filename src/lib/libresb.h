/* library for the service resouce broker
 */

#if !defined(_LIBRESB_H_)
#define _LIBRESB_H_

#define MAX_NAME_LEN 128

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "my_syslog.h"
#include "link.h"
#include "ini.h"

#define ERR (strerror(errno))
/* Max epoll events
 */
#define MAX_EVENTS 1024

typedef enum {
    BROKER_STATUS,
    BROKER_QUEUE_STATUS
} request_t;

typedef enum {
    BROKER_OK,
    BROKER_ERROR
} reply_t;

struct rb_request {
    request_t op;
    char *buf_request;
    int len;
};

struct rb_reply {
    reply_t op_status;
    char *buf_reply;
    int len;
};

struct rb_server {
    char name[MAX_NAME_LEN];
    char *ip;
    int port;
};

extern int get_server_data(struct rb_server *);
extern int nio_client_rw(struct rb_server *,
			 struct rb_request *,
			 struct rb_reply *);

#endif
