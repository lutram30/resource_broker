/* The library for the service resouce broker
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
#include <syslog.h>
#include <ctype.h>
#include <netdb.h>
#include <linux/limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../resb.h"
#include "link.h"
#include "ini.h"

#define ERR (strerror(errno))
/* Max epoll events
 */
#define MAX_EVENTS 1024

/* Request to broker
 */
enum {
    BROKER_STATUS,
    BROKER_QUEUE_STATUS,
    BROKER_PARAMS,
    BROKER_SERVER_REGISTER
};

/* Reply from broker or message to client
 */
enum {
    BROKER_OK,
    BROKER_ERROR,
    BROKER_VM_START,
    BROKER_VM_STOP,
    BROKER_CONTAINER_START,
    BROKER_CONTAINER_STOP
};

/* Message header for all broker communications
 */
struct rb_header {
    int opcode;
    size_t len;
};

/* Messages between broker and clients
 */
struct rb_message {
    struct rb_header *header;
    char *msg_buf;
};

/* rbserver status
 */
typedef enum {
    SERVER_REGISTERED,
    SERVER_UNREGISTERED
} server_stat_t;

struct rb_server {
    int socket;
    char *name;
    server_stat_t type;
    int max_instances;
};

extern int nio_client_rw(struct rb_daemon_id *,
			 struct rb_message *,
			 struct rb_message *);
extern char *remote_addr(int);
extern char *resolve_name(const char *);

#endif
