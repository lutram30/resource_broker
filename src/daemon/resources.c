/* Handle request and return of resources
 */

#include "res.h"

static int run_simulator(struct queue *);
static int get_pend_jobs_sim(const char *);
static int get_num_machines(struct queue *);
static int call_server(struct queue *);

void
check_queue_workload(void)
{
    link_t *l;
    struct queue *q;

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

    /* Count the number of nodes already in the queue
     */
    if (LINK_NUM_ENT(q->machines) >= nmach) {
        syslog(LOG_INFO, "%s: queue %s has already enough machines %d", __func__,
               q->name, LINK_NUM_ENT(q->machines));
        return 0;
    }

    call_server(q);

    return 0;
}

static int
get_pend_jobs_sim(const char *name)
{
    FILE *fp;
    int jobs;

    fp = fopen(name, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "%s: failed oepn simulation file %s %m", __func__, name);
        return -1;
    }

    fscanf(fp, "%d", &jobs);

    fclose(fp);

    return jobs;
}

static int
get_num_machines(struct queue *q)
{
    int nmach;

    nmach = q->num_pend_jobs/q->borrow_factor[0];
    return nmach * q->borrow_factor[1];
}

static int
call_server(struct queue *q)
{
    syslog(LOG_INFO, "%s: queue %s find %d machines of type %s", __func__,
           q->name, q->borrow_factor[1], srv_type2str(q->type));

    return 0;
}
