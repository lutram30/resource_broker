/*
 */

/* The client interface
 */
#include "../resb.h"
#include "../lib/libresb.h"

static void
help()
{
    fprintf(stderr, "rbparams: [-h] -s ip@port");
}

int
main(int argc, char **argv)
{
    int cc;
    char *buf;
    struct rb_daemon_id *d;

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

    buf = rb_broker_params(d);
    if (buf == NULL) {
	fprintf(stderr, "rbparams: I/O error with resource broker\n");
	return -1;
    }

    printf("%s\n", buf);

    free_daemon_id(d);
    free(buf);

    return 0;
}
