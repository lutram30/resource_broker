
#include "rbsrv.h"

/* The broker wants us to allocate some resources
 */
static int boot_resources(int, struct rb_header *);
/* The broker wants us shutdown some resources
 */
static int shut_resources(int, struct rb_header *);

/* Send a registration message to the broker. The rbserver keeps
 * a permanent connection with the broker and receives messages
 * on the tcp/ip connection with it.
 */
int
register_with_broker(struct rb_daemon_id *id)
{
    char buf[SBUFSIZ];
    int s;
    int cc;

    /* Connect to the broker so it will accept this connection
     */
    s = nio_client_init(id->ip, id->port);
    if (s < 0) {
    }
    /* This is the registration messahe with the type of machines this
     * server works with, vm, containers or cloud, its name and the
     * number of available resources.
     */
    sprintf(buf, "%d %s %d", srv->type, srv->name, srv->num_machines);

    /* The header of the message
     */
    struct rb_header hdr;
    hdr.opcode = BROKER_SERVER_REGISTER;
    hdr.len = strlen(buf);

    /* The message itself
     */
    struct rb_message msg;
    msg.header = &hdr;
    msg.msg_buf = buf;

    /* Write to the broker the header with the registration information
     * we are sending
     */
    cc = nio_writeblock(s, &hdr, sizeof(struct rb_header));
    if (cc < 0) {
        close(s);
        return -1;
    }
    /* Send the actual registration message
     */
    cc = nio_writeblock(s, msg.msg_buf, hdr.len);
    if (cc < 0) {
        close(s);
        return -1;
    }

    /* Read the response
     */
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

    /* If the registration with the broker wne all right, we want
     * to put our socket in events array to get further instructions
     * from the broker
     */
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
        /* Look at the return code of the operation with the
         * broker
         */
        int cc = process_broker_request(ev[cc].data.fd);
        if (cc < 0) {
            return -1;
        }
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
        close(s);
        nio_epoll_del(s);
        return -1;
    }

    syslog(LOG_INFO, "%s: server got request %s", __func__,
           srv_req2str(hdr.opcode));

    switch (hdr.opcode) {
    case SERVER_BOOT_RESOURCES:
        boot_resources(s, &hdr);
        break;
    case SERVER_SHUT_RESOURCES:
        shut_resources(s, &hdr);
        break;
    default:
        break;
    }

    return 0;
}

static int
boot_resources(int s, struct rb_header *hdr)
{
    syslog(LOG_INFO, "%s: broker requesting %ld machines", __func__, hdr->len);

    int n = MIN(srv->num_machines, hdr->len);
    char buf[BUFSIZ/8] = {0};
    int i;

    /* Number of host we can send to the broker
     */
    sprintf(buf, "%d ", n);
    for (i = 0; i < n; i++) {
        /* Encode and send the hostnames, we could count and allocate
         * memory for this but we think that BUFSIZ/8 is enough. If not
         * core dump and we will fix it.
         */
        sprintf(buf + strlen(buf), " %s", srv->m[i].name);
    }

    struct rb_header hdr2;
    hdr2.opcode = SERVER_RESOURCES_OK;
    hdr2.len = strlen(buf);

    int cc = nio_writeblock(s, &hdr2, sizeof(struct rb_header));
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

static int
shut_resources(int s, struct rb_header *hdr)
{
    return 0;
}
