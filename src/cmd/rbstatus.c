/*
 */

#include "../resb.h"
#include "../lib/libresb.h"

int
main(int argc, char **argv)
{
    struct rb_message req;
    struct rb_message rep;
    struct rb_header *hdr;
    struct rb_header *hdr2;
    int cc;
    struct rb_server r;
    struct rb_status s;

    cc = get_server_data(&r);
    if (cc < 0) {
	return -1;
    }

    hdr = calloc(1, sizeof(struct rb_header));
    hdr->opcode = BROKER_STATUS;
    hdr->len = 0;
    req.header = hdr;

    cc = nio_client_rw(&r, &req, &rep);
    if (cc < 0) {
	free(hdr);
	return -1;
    }

    hdr2 = rep.header;
    if (hdr2->opcode != BROKER_OK) {
	return -1;
    }

    /* Decode and print
     */
    sscanf(rep.msg_buf, "%ld %d %lf %lf %lf", &s.uptime, &s.pid,
	   &s.load[0], &s.load[1], &s.load[2]);

    char *t = ctime(&s.uptime);
    printf("broker status: ok\n");
    printf("  up since: %.19s, pid: %d, load average: %lf, %lf, %lf\n",
	   t, s.pid, s.load[0], s.load[1], s.load[2]);

    return 0;
}
