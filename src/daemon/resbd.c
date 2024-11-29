/* Daemon of the service resource broker
 */

/* Resource brokers header file
 */
#include "res.h"

/* Interface to nio_epoll() together with nready and the ms based
 * timer
 */
static struct epoll_event events[MAX_EVENTS];
static struct epoll_event ev;
static char debug;

/* Daemon structures
 */
struct resbd *res;
struct parameters *params;
link_t *queues;
link_t *servers;
time_t my_uptime;

static int init_main_data(void);
static void manage_resources(void);
static int handle_events(int, struct epoll_event *);
static void handle_connection(int);
static void open_log(void);
static void dump_config(void);

static void usage(void)
{
    fprintf(stderr, "resbd: [-h print this help] [-v prints version] \
[-f set configuration file. default is /usr/local/etc/resdb.conf] \
[-d debug]\n");
}

int
main(int argc, char **argv)
{
    int cc;
    char *cfile;

    /* Set default value first
     */
    cfile = "/usr/local/etc/resdb.conf";
    debug = 0;

    while ((cc = getopt(argc, argv, "hvf:d")) != EOF) {
        switch (cc) {
        case 'v':
            printf("RESBD_VERSION\n");
            return 0;
        case 'f':
            cfile = optarg;
            break;
        case 'd':
            debug = 1;
            break;
        case '?':
        default:
            usage();
            return -1;
        }
    }

    init_main_data();
    /* Read the configuration and build the basic data structure
     */
    read_config(cfile);

    open_log();

    dump_config();

    /* Initialize the network layer
     */
    res->sock = nio_server_init(res->port);
    syslog(LOG_INFO, "%s: daemon started on port %d", __func__, res->port);

    while (1) {
        int nready;

        nready = nio_epoll(events, res->epoll_timer);
        if (nready < 0) {
            syslog(LOG_ERR, "resdb: network I/O error reported by epoll %m");
            continue;
        }

        handle_events(nready, events);

        manage_resources();
    }

    return 0;
}

static int
init_main_data(void)
{
    res = calloc(1, sizeof(struct resbd));
    res->epoll_timer = EPOLL_TIMER * 1000;

    params = calloc(1, sizeof(struct parameters));

    queues = link_make();
    servers = link_make();

    time(&my_uptime);

    return 0;
}

static void
open_log(void)
{
    int mask;

    if (params->log_mask == NULL)
        params->log_mask = strdup("LOG_INFO");

    if (strcmp(params->log_mask, "LOG_ERR") == 0)
        mask = LOG_ERR;
    else if (strcmp(params->log_mask, "LOG_WARNING") == 0)
        mask = LOG_WARNING;
    else if (strcmp(params->log_mask, "LOG_INFO") == 0)
        mask = LOG_INFO;
    else if (strcmp(params->log_mask, "LOG_DEBUG") == 0)
        mask = LOG_DEBUG;
    else
        mask = LOG_INFO;

    setlogmask(LOG_UPTO(mask));

    if (debug)
        openlog("resbd", LOG_NDELAY|LOG_PERROR|LOG_PID, LOG_DAEMON);
    else
        openlog("resbd", LOG_NDELAY|LOG_PID, LOG_DAEMON);
}

static int
handle_events(int nready, struct epoll_event *e)
{
    int cc;

    for (cc = 0; cc < nready; cc++) {
        socklen_t addrlen;
        struct sockaddr_in addr;
        struct s;
        int as;

        if (events[cc].data.fd == res->sock) {
            as = accept(res->sock,
                        (struct sockaddr *)&addr, &addrlen);
            if (as == -1) {
                syslog(LOG_ERR, "%s: accept() failed %m", __func__);
                return -1;
            }

            /* Block as we want to read everything in the next
             * read call
             */
            nio_block(as);

            ev.events = EPOLLIN;
            ev.data.fd = as;

            nio_epoll_add(as, &ev);

            syslog(LOG_DEBUG, "%s: connection accepted %d", __func__, as);

            continue;
        }

        if (events[cc].events & (EPOLLERR | EPOLLHUP)) {
            syslog(LOG_ERR, "%s: epoll error on connection with %s", __func__,
                   remote_addr(events[cc].data.fd));
            close(events[cc].data.fd);
            nio_epoll_del(events[cc].data.fd);
            continue;
        }
        /* Event is in read
         */
        if (events[cc].events & EPOLLIN) {
            handle_connection(events[cc].data.fd);
        }
    }

    return 0;
}

static void
handle_connection(int s)
{
    struct rb_header hdr;
    ssize_t cc;

    cc = nio_readblock(s, &hdr, sizeof(struct rb_header));
    if (cc < 0) {
        syslog(LOG_ERR, "%s: failed reading header from %s",
               __func__, remote_addr(s));
        nio_epoll_del(s);
        close(s);
        return;
    }

    switch (hdr.opcode) {
    case BROKER_STATUS:
        status_info(s, &hdr);
        nio_epoll_del(s);
        close(s);
        break;
    case BROKER_PARAMS:
        params_info(s, &hdr);
        nio_epoll_del(s);
        close(s);
        break;
    case BROKER_QUEUE_STATUS:
        queue_info(s, &hdr);
        nio_epoll_del(s);
        close(s);
        break;
    case BROKER_SERVER_REGISTER:
        cc = server_register(s, &hdr);
        if (cc > 0) {
            syslog(LOG_ERR, "%s: failed to register server at %d %s",
                   __func__, s, remote_addr(s));
            nio_epoll_del(s);
            close(s);
        }
        break;
    default:
        break;
    }

    return;

}

static void
manage_resources(void)
{
    static time_t last;
    char buf[64];
    time_t t = time(NULL);

    syslog(LOG_INFO, "%s: processing", __func__);

    if (last == 0) {
        last = t;
        check_queue_workload();
        return;
    }

    if (! (t - last >= RESOURCE_DETECT_TIMER))
        return;

    last = t;

    syslog(LOG_INFO, "%s: now %s check need for compute resources", __func__,
           my_time(buf));

    check_queue_workload();
}

static void
dump_config(void)
{
    /* Dump params
     */

    /* Dump queues
     */
    link_t *l;
    for (l = queues->next; l != NULL; l = l->next) {
        struct queue *q = (struct queue *)l->ptr;
        syslog(LOG_INFO, "%s: queue:%s status:%d name_space:%s borrow factor: %d,%d",
               __func__, q->name, q->status, q->name_space, q->borrow_factor[0],
               q->borrow_factor[1]);
    }

}
