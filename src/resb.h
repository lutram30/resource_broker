/* Service Resource Broker user API
 * Define users' API
 */

#if !defined(_RESB_API_H_)
#define _RESB_API_H_

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

/* Helper data structure giving the network address
 * of the resbd daemon
 */
struct rb_daemon_id {
    char *ip;
    int port;
};

/* Current status of the resource broker
 */
struct rb_status {
    time_t uptime;
    pid_t pid;
    double load[3];
};

extern struct rb_daemon_id *get_daemon_id(char *);
extern void free_daemon_id(struct rb_daemon_id *);
extern struct rb_status *rb_broker_status(struct rb_daemon_id *r);
extern char *rb_broker_params(struct rb_daemon_id *);
extern char *my_time(char *);

#endif
