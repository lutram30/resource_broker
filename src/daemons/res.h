/* Service broker daemons header files
 */

#include "../lib/libresb.h"
#include "../lib/nio.h"

#define RESBD_VERSION "0.1"
#define MAX_QUEUES 10

/* Resource Broker Daemon
 */
struct resbd {
    char host[MAX_NAME_LEN];
    int port;
    int sock;
    int epoll_timer; /* milliseconds */
};
extern struct resbd *res;

struct params {
    char scheduler_type[MAX_NAME_LEN];
    char conf_dir[PATH_MAX];
    char work_dir[PATH_MAX];
    char container_runtime[MAX_NAME_LEN];
    char vm_runtime[MAX_NAME_LEN];
    int workload_timer;
};
extern struct params *prms;

typedef enum {
    MACHINE_VMS,
    MACHINE_CONTAINERS
} mach_t;

typedef enum {
    MACHINE_BORROWED,
    MACHINE_IDLE
} mach_stat_t;

struct machine {
    char name[MAX_NAME_LEN];
    int type;
    mach_stat_t status;
};

typedef enum {
    QUEUE_STAT_IDLE,
    QUEUE_STAT_BORROWING,
    QUEUE_STAT_BORROW,
    QUEUE_STAT_RETURNING
} queue_stat_t;

struct queue {
    char name[MAX_NAME_LEN];
    queue_stat_t status;
    mach_t type;
    char name_space[MAX_NAME_LEN];
    link_t *machines;
    /* Every borrow_factor[0] add borrow_factor[1] hosts/containers
     */
    int borrow_factor[2];
};
extern struct queue *rqueue;
extern link_t *queues;

extern int read_config(const char *);
