
#include "rbsrv.h"

int
register_with_broker(struct rb_daemon_id *id)
{
    char buf[1024];
    int i;
    int s;
    int cc;

    s = nio_client_init(id->ip, id->port);

    sprintf(buf, "%d %d %d", s, srv->type, srv->num_machines);
    for (i = 0; i < srv->num_machines; i++) {
	sprintf(buf + strlen(buf), " %s", srv->m[i].name);
    }

    struct rb_header hdr;
    hdr.opcode = BROKER_SERVER_REGISTER;
    hdr.len = strlen(buf);

    struct rb_message msg;
    msg.header = &hdr;
    msg.msg_buf = buf;

    /* Write to client
     */
    cc = nio_writeblock(s, &hdr, sizeof(struct rb_header));
    if (cc < 0) {
	close(s);
	return -1;
    }
    cc = nio_writeblock(s, msg.msg_buf, hdr.len);
    if (cc < 0) {
	close(s);
	return -1;
    }

    struct rb_header hdr2;;
    cc = nio_readblock(s, &hdr2, sizeof(struct rb_header));
    if (cc < 0) {
	close(s);
	return -1;
    }

    if (hdr2.opcode != BROKER_OK) {
	syslog(LOG_ERR, "%s: network i/o error with broker", __func__);
	close(s);
	return -1;
    }

    return s;
}


int
process_broker_request(int s, struct epoll_event *ev)
{

    return 0;
}
