/* Service Resource Broker API
 * Define users' API
 */

#if !defined(_RESB_API_H_)
#define _RES_API_H_

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

struct rb_status {
    time_t uptime;
    pid_t pid;
    double load[3];
};
extern struct rb_status *rb_broker_status(void);

#endif
