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

/* Daemon structures
 */
struct resbd *res;
struct params *prms;
link_t *queues;
time_t my_uptime;

static int init_main_data(void);
static void manage_resources(void);
static int handle_events(int, struct epoll_event *);
static void handle_connection(int);

static void usage(void)
{
    fprintf(stderr, "resbd: [-v prints version] \
[-f set configuration file. default is /usr/local/etc/resdb.conf]\n");
}

int
main(int argc, char **argv)
{
    int cc;
    char *cfile;

    /* Set default value first
     */
    cfile = "/usr/local/etc/resdb.conf";

    while ((cc = getopt(argc, argv, "vf:")) != EOF) {
	switch (cc) {
	case 'v':
	    printf("RESBD_VERSION\n");
	    return 0;
	case 'f':
	    cfile = optarg;
	    break;
	case '?':
	default:
	    usage();
	    return -1;
	}
    }

    init_main_data();
    /* Read the configuration and buiild the basic data structure
     */
    read_config(cfile);

    /* Initialize the network layer
     */
    res->sock = nio_server_init(res->port);

    while (1) {
	int nready;
	int ms = -1;

	nready = nio_epoll(events, ms);
	if (nready < 0) {
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
    res->epoll_timer = -1; /* ms */

    prms = calloc(1, sizeof(struct params));

    queues = link_make();

    time(&my_uptime);

    return 0;
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
	    }

	    /* Block as we want to read everything in the next
	     * read call
	     */
	    nio_block(as);

	    ev.events = EPOLLIN;
	    ev.data.fd = as;

	    nio_epoll_add(as, &ev);
	    continue;
	}
	handle_connection(events[cc].data.fd);
    }

    return 0;
}

static void
handle_connection(int s)
{
    struct rb_header hdr;
    ssize_t cc;

    nio_epoll_del(s);

    cc = nio_readblock(s, &hdr, sizeof(struct rb_header));
    if (cc < 0) {
    }

    switch (hdr.opcode) {
    case BROKER_STATUS:
	handle_broker_status(s, &hdr);
	break;
    default:
	break;
    }

    close(s);

    return;

}

static void
manage_resources(void)
{
    return;
}
