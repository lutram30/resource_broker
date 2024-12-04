/* Handle request and return of resources
 */

#include "res.h"

static int run_simulator(struct queue *);
static int get_pend_jobs_sim(const char *);
static int get_num_machines(struct queue *);
static int get_server_resources(struct queue *, int);

void
check_queue_workload(void)
{
    link_t *l;
    struct queue *q;

    if (LINK_NUM_ENT(servers) == 0) {
        syslog(LOG_INFO, "%s: no servers are registered yet", __func__);
        return;
    }

    for (l = queues ->next; l != NULL; l = l->next) {
        q = (struct queue *)l->ptr;
        syslog(LOG_INFO, "%s: look at queue %s", __func__, q->name);
        if (simulate)
            run_simulator(q);
    }
}

static int
run_simulator(struct queue *q)
{
    int nmach;
    int pend_jobs;

    pend_jobs = get_pend_jobs_sim(q->name);
    if (pend_jobs < 0) {
        return -1;
    }
    q->num_pend_jobs = pend_jobs;

    syslog(LOG_INFO, "%s: queue %s has %d num_pend_jobs", __func__, q->name,
           q->num_pend_jobs);

    nmach = get_num_machines(q);
    syslog(LOG_INFO, "%s: queue %s needs %d machines", __func__, q->name, nmach);

    if (nmach == 0)
        return 0;

    get_server_resources(q, nmach);

    return 0;
}

static int
get_pend_jobs_sim(const char *name)
{
    FILE *fp;
    int jobs;

    fp = fopen(name, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "%s: failed open simulation file %s %m", __func__, name);
        return -1;
    }

    fscanf(fp, "%d", &jobs);

    fclose(fp);

    return jobs;
}

static int
get_num_machines(struct queue *q)
{
    int num;
    int nmach;

    nmach = q->num_pend_jobs/q->borrow_factor[0];
    if (nmach == 0) {
        syslog(LOG_INFO, "%s: queue %s does not need machines",
               __func__, q->name);
        return 0;
    }
    num = nmach * q->borrow_factor[1];

    /* Count the number of nodes already in the queue
     */
    if (LINK_NUM_ENT(q->machines) >= num) {
        syslog(LOG_INFO, "%s: queue %s has already enough machines %d", __func__,
               q->name, LINK_NUM_ENT(q->machines));
        return 0;
    }

    int num_need = num - LINK_NUM_ENT(q->machines);

    return num_need;
}

static int
get_server_resources(struct queue *q, int num)
{
    struct server *s;
    link_t *l;

    syslog(LOG_INFO, "%s: queue %s wants %d machines of type %s", __func__,
           q->name, q->borrow_factor[1], srv_type2str(q->type));

    for (l = servers->next; l != NULL; l = l->next) {
        s = (struct server *)l->ptr;
        break;
    }

    struct rb_header hdr;
    hdr.opcode = SERVER_GET_RESOURCES;
    hdr.len = num;

    int cc = nio_writeblock(s->socket, &hdr, sizeof(struct rb_header));
    if (cc < 0) {
        syslog(LOG_ERR, "%s: failed to write to server %s %m", __func__, s->name);
        return -1;
    }

    struct rb_header hdr2;
    cc = nio_readblock(s->socket, &hdr2, sizeof(struct rb_header));
    if (cc < 0) {
        syslog(LOG_ERR, "%s: failed reading header from broker %m", __func__);
        return -1;
    }

    syslog(LOG_INFO, "%s: broker starting %ld machines", __func__, hdr2.len);
    char  buf[1024] = {0};

    cc = nio_readblock(s->socket, buf, hdr.len);
    if (cc < 0) {
        syslog(LOG_ERR, "%s: failed reading buffer from broker %m", __func__);
        return -1;
    }

    char machine[MAXHOSTNAMELEN];
    int i;
    for (i = 0; i < hdr2.len; i++) {
        sscanf(buf + strlen(buf), "%s ", machine);
        syslog(LOG_INFO, "%s: machine: %s", __func__, machine);
    }

    return 0;
}
