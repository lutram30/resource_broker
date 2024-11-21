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

#include "link.h"
#include "ini.h"

#define ERR (strerror(errno))
/* Max epoll events
 */
#define MAX_EVENTS 1024

enum {
    BROKER_STATUS,
    BROKER_QUEUE_STATUS
};

enum {
    BROKER_OK,
    BROKER_ERROR
};

struct rb_header {
    int opcode;
    size_t len;
};

struct rb_message {
    struct rb_header *header;
    char *msg_buf;
};

struct rb_server {
    char name[MAX_NAME_LEN];
    char *ip;
    int port;
};

extern int get_server_data(struct rb_server *);
extern int nio_client_rw(struct rb_server *,
			 struct rb_message *,
			 struct rb_message *);

#endif
