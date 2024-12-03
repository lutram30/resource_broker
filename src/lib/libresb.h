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

/* Reply from broker or message from broker to client
 */
enum {
    BROKER_OK,
    BROKER_ERROR,
    BROKER_VM_START,
    BROKER_VM_STOP,
    BROKER_CONTAINER_START,
    BROKER_CONTAINER_STOP
};

struct rb_broker {
    char *name;
    int port;
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

enum {
    SERVER_TYPE_VM,
    SERVER_TYPE_CONTAINER,
    SERVER_TYPE_CLOUD
};

enum {
    SERVER_STATUS_OK,
    SERVER_STATUS_ALLOCATED,
    SERVER_STATUS_BUSY
};

enum {
    MACHINE_VMS,
    MACHINE_CONTAINERS,
    MACHINE_CLOUD
};

enum {
    MACHINE_ON,
    MACHINE_OFF
};

struct rb_machine {
    char *name;
    int type;
    int status;
};

struct rb_server {
    int socket;
    char *name;
    int type;
    int status;
    int num_machines;
    struct rb_machine *m;
};

extern ssize_t nio_client_rw(struct rb_daemon_id *,
			     struct rb_message *,
			     struct rb_message *);
extern char *remote_addr(int);
extern char *resolve_name(const char *);
extern struct rb_broker *get_broker(const char *);
extern char *srv_type2str(int);

#endif
