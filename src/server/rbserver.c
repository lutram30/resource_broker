
#include "rbsrv.h"

/* Global server seen in any module
 */
struct rb_server *srv;
/* Global debug flag seen in every module fundamental to trace
 * the system
 */
int debug;

/* Local array for epoll only one message at time anyway...
 */
static struct epoll_event events[MAX_SRV_EVENTS];

static void open_server_log(const char *);
static int process_machine_file(const char *);
static void periodic(void);
static void dump_srv(void);
static const char *srv_type2str(int);

static void
help(void)
{
    fprintf(stderr, "rbserver -s ip@port -t [vm|container...] "
	    "-m nachinefile [-d debug classes log_info|log_err|..]\n");
}

int
main(int argc, char **argv)
{
    int cc;
    struct rb_daemon_id *id;

    srv = calloc(1, sizeof(struct rb_server));

    while ((cc = getopt(argc, argv, "d:ht:s:m:")) != EOF) {
	switch (cc) {
	case 't':
	    if (strcasecmp(optarg, "vm") == 0)
		srv->type = SERVER_TYPE_VM;
	    else if (strcasecmp(optarg, "container") == 0)
		srv->type = SERVER_TYPE_CONTAINER;
	    else if (strcasecmp(optarg, "cloud") == 0)
		srv->type = SERVER_TYPE_CLOUD;
	    else {
		fprintf(stderr, "%s: unknown server type\n", __func__);
		help();
		return -1;
	    }
	break;
	case 's':
	    id = get_daemon_id(optarg);
	    break;
	case 'm':
	    cc = process_machine_file(optarg);
	    if (cc < 0) {
		return -1;
	    }
	    break;
	case 'd':
	    debug = 1;
	    open_server_log(optarg);
	    break;
	case '?':
	case 'h':
	    help();
	    return -1;
	}
    }

    dump_srv();

    srv->socket = register_with_broker(id);
    if (cc < 0) {
	syslog(LOG_ERR, "rbserver; failed to register with broker %m");
	close(srv->socket);
	return -1;
    }

    while (1) {
	int ms = 20 * 1000;

	cc = nio_epoll2(events, MAX_SRV_EVENTS, ms);
	if (cc < 0) {
	    syslog(LOG_ERR, "rbserver: nio_epoll() error %m");
	    continue;
	}

	if (cc == 0) {
	    periodic();
	    continue;
	}

	/* cc must be one
	 */
	process_broker_request(cc, events);
    }

    return 0;
}

static void
periodic(void)
{
    /* Do nothing for now
     */
}

static int
process_machine_file(const char *name)
{
    FILE *fp;
    int n;
    int i;
    char buf[64];

    fp = fopen(name, "r");
    if (fp == NULL) {
	syslog(LOG_ERR, "%s: error opening file %s %m", __func__, name);
	return -1;
    }

    n = 0;
    while (fgets(buf, sizeof(buf), fp))
	++n;
    rewind(fp);

    i = 0;
    srv->num_machines = n;
    srv->m = calloc(n, sizeof(struct rb_machine));
    while (fgets(buf, sizeof(buf), fp)) {
	buf[strlen(buf) - 1] = 0;
	srv->m[i].name = strdup(buf);
	srv->m[i].type = srv->type;
	srv->m[i].status = MACHINE_OFF;
	++i;
    }
    fclose(fp);

    return 0;
}


static void
open_server_log(const char *level)
{
    int mask;

    if (level == NULL)
	level = strdup("LOG_INFO");

    if (strcmp(level, "LOG_ERR") == 0)
	mask = LOG_ERR;
    else if (strcmp(level, "LOG_WARNING") == 0)
	mask = LOG_WARNING;
    else if (strcmp(level, "LOG_INFO") == 0)
	mask = LOG_INFO;
    else if (strcmp(level, "LOG_DEBUG") == 0)
	mask = LOG_DEBUG;
    else
	mask = LOG_INFO;

    setlogmask(LOG_UPTO(mask));

    if (debug)
	openlog("rbserver", LOG_NDELAY|LOG_PERROR|LOG_PID, LOG_DAEMON);
    else
	openlog("rbserver", LOG_NDELAY|LOG_PID, LOG_DAEMON);
}

static void
dump_srv(void)
{
    syslog(LOG_INFO, "%s; server has socket %d serving type %s", __func__,
	   srv->socket, srv_type2str(srv->type));
}

static const char *
srv_type2str(int type)
{
    if (srv->type == SERVER_TYPE_VM)
	return "SERVER_TYPE_VM";
    if (srv->type == SERVER_TYPE_CONTAINER)
	return "SERVER_TYPE_CONTAINER";
    if (srv->type == SERVER_TYPE_CLOUD)
	return "SERVER_TYPE_CLOUD";

    return "Unknown type";
}
