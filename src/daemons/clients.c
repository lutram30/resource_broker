/* Handle client messages
 */

#include "res.h"

int
handle_broker_status(int s, struct rb_header *hdr)
{
    struct rb_header hdr2;
    char buf[64];
    int cc;
    double load[3] = {0.0};

    getloadavg(load, 3);

    /* Client has to decode this
     */
    sprintf(buf, "%ld %d %lf %lf %lf", my_uptime, getpid(),
	    load[0], load[1], load[2]);

    hdr2.opcode = BROKER_OK;
    hdr2.len = strlen(buf);

    cc = nio_writeblock(s, &hdr2, sizeof(struct rb_header));
    if (cc < 0) {
	return -1;
    }

    cc = nio_writeblock(s, buf, hdr2.len);
    if (cc < 0) {
	return -1;
    }

    close(s);

    return 0;
}
