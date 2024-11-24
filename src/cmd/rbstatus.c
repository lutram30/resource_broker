/* Status of the broker daemon
 */

#include "../resb.h"
#include "../lib/libresb.h"

static void
help()
{
    fprintf(stderr, "rbstatus: [-h] -s ip@port");
}

int
main(int argc, char **argv)
{
    int cc;
    struct rb_daemon_id *d;
    struct rb_status *s;

    while ((cc = getopt(argc, argv, "hs;")) != EOF) {
	switch (cc) {
	case 's':
	    d = get_daemon_id(optarg);
	    break;
	case '?':
	case 'v':
	    help();
	    return -1;
	}
    }

    s = rb_broker_status(d);
    if (s == NULL) {
	fprintf(stderr, "rbstats: I/O error with resource broker\n");
	free_daemon_id(d);
	return -1;
    }

    char *t = ctime(&s->uptime);
    printf("broker status: ok\n");
    printf("  up since: %.19s, pid: %d, load average: %lf, %lf, %lf\n",
	   t, s->pid, s->load[0], s->load[1], s->load[2]);

    free(s);
    free_daemon_id(d);

    return 0;
}
