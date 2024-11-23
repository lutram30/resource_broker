/* Service broker daemons header files
 */

#include "../lib/libresb.h"
#include "../lib/nio.h"

#define RESBD_VERSION "0.1"
#define MAX_QUEUES 10

/* Resource Broker Daemon
 */
struct resbd {
    char *host;
    int port;
    int sock;
    int epoll_timer; /* milliseconds */
};
extern struct resbd *res;
extern time_t my_uptime;

struct parameters {
    char *scheduler;
    char *work_dir;
    char *container_runtime;
    char *vm_runtime;
    int workload_timer;
    char *log_mask;
};

extern struct parameters *params;

typedef enum {
    MACHINE_VMS,
    MACHINE_CONTAINERS,
    MACHINE_CLOUD
} mach_t;

struct machine {
    char *name;
    int type;
};

typedef enum {
    QUEUE_STAT_IDLE,
    QUEUE_STAT_BORROWING,
} queue_stat_t;

struct queue {
    char *name;
    queue_stat_t status;
    mach_t type;
    char *name_space;
    link_t *machines;
    /* Every borrow_factor[0] add borrow_factor[1] hosts/containers
     */
    int borrow_factor[2];
};
extern struct queue *rqueue;
extern link_t *queues;

extern int read_config(const char *);
extern int status_info(int, struct rb_header *);
extern int params_info(int, struct rb_header *);
extern int queue_info(int, struct rb_header *);
