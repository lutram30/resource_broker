/* Service broker daemons header files
 */

#include "../lib/libresb.h"
#include "../lib/nio.h"

#define RESBD_VERSION "0.1"
#define MAX_QUEUES 10
#define RESOURCE_DETECT_TIMER 10
#define EPOLL_TIMER 10

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
extern char simulate;

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
    QUEUE_STAT_IDLE,
    QUEUE_STAT_BORROWING,
} queue_stat_t;

struct queue {
    char *name;
    queue_stat_t status;
    int type;           /* machine types */
    char *name_space;
    link_t *machines;
    /* Every borrow_factor[0] add borrow_factor[1] hosts/containers
     */
    int borrow_factor[2];
    int num_pend_jobs;
};

/* server status
 */
typedef enum {
    SERVER_REGISTERED,
    SERVER_UNREGISTERED
} server_stat_t;

struct server {
    int socket;
    char *name;
    server_stat_t status;
    int type;
    int max_instances;
};

extern link_t *queues;
extern link_t *servers;

extern int read_config(const char *);
extern ssize_t status_info(int, struct rb_header *);
extern ssize_t params_info(int, struct rb_header *);
extern ssize_t queue_info(int, struct rb_header *);
extern ssize_t server_register(int, struct rb_header *);
extern void check_queue_workload(void);
extern void free_server(struct server *);
