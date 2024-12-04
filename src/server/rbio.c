
#include "rbsrv.h"

int
register_with_broker(struct rb_daemon_id *id)
{
    char buf[1024];
    int i;
    int s;
    int cc;

    s = nio_client_init(id->ip, id->port);

    sprintf(buf, "%d %s %d", srv->type, srv->name, srv->num_machines);
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

    struct epoll_event ev;

    ev.events = EPOLLIN;
    ev.data.fd = s;
    nio_block(s);

    nio_epoll_add(s, &ev);

    return s;
}

int
handle_broker_events(int nready, struct epoll_event *ev)
{
    int cc;

    for (cc = 0; cc < nready; cc++) {
        process_broker_request(ev[cc].data.fd);
    }

    return 0;
}

int
process_broker_request(int s)
{
    struct rb_header hdr;
    int cc;

    cc = nio_readblock(s, &hdr, sizeof(struct rb_header));
    if (cc < 0) {
        syslog(LOG_ERR, "%s: failed reading header from broker %m", __func__);
        return -1;
    }

    syslog(LOG_INFO, "%s: broker requesting %ld machines", __func__, hdr.len);
    int n = MIN(srv->num_machines, hdr.len);
    char buf[1024] = {0};
    int i;

    for (i = 0; i < n; i++) {
        sprintf(buf + strlen(buf), " %s", srv->m[i].name);
    }

    struct rb_header hdr2;
    hdr2.opcode = SERVER_RESOURCES_OK;
    hdr2.len = strlen(buf);

    cc = nio_writeblock(s, &hdr2, sizeof(struct rb_header));
    if (cc < 0) {
        syslog(LOG_ERR, "%s: failed writing header to broker %m", __func__);
         return -1;
    }

    cc = nio_writeblock(s, buf, hdr2.len);
    if (cc < 0) {
        syslog(LOG_ERR, "%s; failed writing buf %s to broker %m", __func__, buf);
         return -1;
    }

    return 0;
}
