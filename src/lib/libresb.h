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

#include "nio.h"
#include "link.h"
#include "ini.h"

typedef enum {
    BROKER_STATUS,
    BROKER_QUEUE_STATUS
} request_t;

typedef enum {
    BROKER_OK,
    BROKE_ERERROR
} reply_t;

struct op_request {
    request_t req;
    char *buf_request;
};

struct op_reply {
    reply_t rep;
    char *reply_buf;
};

#endif
