/* Handle client messages
 */

#include "res.h"

ssize_t
status_info(int s, struct rb_header *hdr)
{
    struct rb_header hdr2;
    char buf[64];
    int cc;
    double load[3] = {0.0};

    syslog(LOG_DEBUG, "%s: processing", __func__);

    getloadavg(load, 3);

    /* Client has to decode this
     */
    sprintf(buf, "%ld %d %lf %lf %lf", my_uptime, getpid(),
	    load[0], load[1], load[2]);

    hdr2.opcode = BROKER_OK;
    hdr2.len = strlen(buf);

    cc = nio_writeblock(s, &hdr2, sizeof(struct rb_header));
    if (cc < 0) {
	syslog(LOG_ERR, "%s: failed writing to %s", __func__, remote_addr(s));
	return -1;
    }

    cc = nio_writeblock(s, buf, hdr2.len);
    if (cc < 0) {
	syslog(LOG_ERR, "%s: failed writing to %s", __func__, remote_addr(s));
	return -1;
    }

    return 0;
}

ssize_t
params_info(int s, struct rb_header *hdr)
{
    char buf[1024];

    syslog(LOG_DEBUG, "%s: processing", __func__);

    sprintf(buf, "system parameters\n");
    if (params->scheduler)
	sprintf(buf + strlen(buf), "  scheduler:%s\n", params->scheduler);
    if (params->work_dir)
	sprintf(buf + strlen(buf), "  workdir:%s\n", params->work_dir);
    if (params->container_runtime)
	sprintf(buf + strlen(buf), "  container_runtime:%s\n",
		params->container_runtime);
    if (params->vm_runtime)
	sprintf(buf + strlen(buf), "  vm_runtime:%s\n", params->vm_runtime);
    sprintf(buf + strlen(buf), "  workload_timer:%d\n", params->workload_timer);
    if (params->log_mask)
	sprintf(buf + strlen(buf), "  log_mask:%s", params->log_mask);

    struct rb_header hdr2;
    hdr2.opcode = BROKER_OK;
    hdr2.len = strlen(buf) + 1;

    int cc = nio_writeblock(s, &hdr2, sizeof(struct rb_header));
    if (cc < 0) {
	syslog(LOG_ERR, "%s: failed writing to %s", __func__, remote_addr(s));
	return -1;
    }

    cc = nio_writeblock(s, buf, hdr2.len);
    if (cc < 0) {
	syslog(LOG_ERR, "%s: failed writing to %s", __func__, remote_addr(s));
	return -1;
    }

    close(s);

    return 0;
}

ssize_t
queue_info(int s, struct rb_header *hdr)
{
    syslog(LOG_DEBUG, "%s: processing", __func__);

    return 0;
}

ssize_t
server_register(int s, struct rb_header *hdr)
{
    struct server *srv;
    char buf[1024];
    ssize_t cc;

    syslog(LOG_DEBUG, "%s: processing", __func__);

    srv = calloc(1, sizeof(struct server));

    cc = nio_readblock(s, buf, hdr->len);
    if (cc < 0) {
	syslog(LOG_ERR, "%s: failed reading %ld bytes from %s %m", __func__,
	       hdr->len, remote_addr(s));
	return -1;
    }

    srv->name = calloc(1, MAX_NAME_LEN);
    sscanf(buf, "%d %s %d", &srv->socket, srv->name, &srv->max_instances);
    srv->status = SERVER_REGISTERED;

    link_enque(servers, srv);

    struct rb_header hdr2;
    hdr2.opcode = BROKER_OK;
    hdr2.len = 0;

    cc = nio_writeblock(s, &hdr2, sizeof(struct rb_header));
    if (cc < 0) {
	syslog(LOG_ERR, "%s: failed sending header to %s %m", __func__,
	       remote_addr(s));
	return -1;
    }

    return 0;
}
