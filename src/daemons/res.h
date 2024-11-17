/* Service broker daemons header files
 */

#include "../lib/libresb.h"

#define RESBD_VERSION "0.1"


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

struct queue {
    char name[MAX_NAME_LEN];
    int state;
    int type;
    char name_space[MAX_NAME_LEN];
    char *machines;
    /* Every borrow_factor[0] add borrow_factor[1] hosts/containers
     */
    int borrow_factor[2];
};
extern link_t *queues;

extern int read_config(const char *);
