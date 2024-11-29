/* Resource broker parsing configuration file
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "res.h"

static int init_resbd(const char *, const char *);
static int init_params(const char *, const char *);
static int init_queue(const char *, const char *, const char *);
static int handler(void *, const char *, const char *, const char *);
static struct queue *get_queue_by_name(const char *);
static void set_default_params(void);

int
read_config(const char *file)
{
    if (ini_parse(file, handler, NULL) < 0) {
        printf("Can't load 'test.ini'\n");
        return -1;
    }

    return 0;
}

static int handler(void *user,
                   const char *section,
                   const char *key,
                   const char *value)
{
    if (strcasecmp(section, "resbd") == 0) {
        return init_resbd(key, value);
    }

    if (strcasecmp(section, "params") == 0) {
        return init_params(key, value);
    }

    if (strstr(section, "queue")) {
        char qname[MAX_NAME_LEN];
        sscanf(section, "%*s%s", qname);
        return init_queue(qname, key, value);
    }

    return 1;
}

static int
init_resbd(const char *key, const char *val)
{
    if (strcasecmp(key, "host") == 0) {
        res->host = strdup(val);
        return 1;
    }
    if (strcasecmp(key, "port") == 0) {
        res->port = atoi(val);
        return 1;
    }

    return 0;
}

static int
init_params(const char *key, const char *val)
{
    set_default_params();

    if (strcasecmp(key, "scheduler") == 0) {
        params->scheduler = strdup(val);
    } else if (strcasecmp(key, "work_dir") == 0) {
        params->work_dir = strdup(val);
    } else  if (strcasecmp(key, "container_runtime") == 0) {
        params->container_runtime = strdup(val);
    } else if (strcasecmp(key, "vm_runtime") == 0) {
        params->vm_runtime = strdup(val);
    } else if (strcasecmp(key, "workload_timer") == 0) {
        params->workload_timer = atoi(val);
    } if (strcasecmp(key, "log_mask") == 0) {
        int i;
        char *C = strdup(val);
        for (i = 0; val[i]; i++)
            C[i] = toupper(val[i]);
        params->log_mask = strdup(C);
    }

    return 1;
}

static void
set_default_params(void)
{
    params->scheduler = strdup("slurm");
    params->container_runtime = strdup("docker");
    params->vm_runtime = strdup("virsh");
    /* Seconds
     */
    params->workload_timer = 30;
    params->log_mask = strdup("LOG_INFO");
}

static int
init_queue(const char *qname, const char *key, const char *val)
{
    struct queue *q;

    q = get_queue_by_name(qname);

    if (strcasecmp(key, "type") == 0) {
        if (strcasecmp(val, "vm") == 0)
            q->type = MACHINE_VMS;
        else
            q->type = MACHINE_CONTAINERS;
    }
    if (strcasecmp(key, "name_space") == 0) {
        q->name_space = strdup(val);
    }
    if (strcasecmp(key, "borrow_factor") == 0) {
        sscanf(val, "%d %d", &q->borrow_factor[0], &q->borrow_factor[1]);
    }
    if (strcasecmp(key, "type") == 0) {
        if (strcasecmp(val, "vm") == 0) {
            q->type = MACHINE_VMS;
        } else if (strcasecmp(val, "container") == 0) {
            q->type = MACHINE_CONTAINERS;
        } else if (strcasecmp(val, "cloud") == 0) {
            q->type = MACHINE_CLOUD;
        } else {
            /* Default is VM
             */
            q->type = MACHINE_VMS;
        }
    }
    return 1;
}

static struct queue *
get_queue_by_name(const char *name)
{
    struct queue *q;
    link_t *l;

    for (l = queues->next; l != NULL; l = l->next) {
        q = (struct queue *)l->ptr;
        if (strcasecmp(name, q->name) == 0) {
            return q;
        }
    }

    /* new queue
     */
    q = calloc(1, sizeof(struct queue));
    q->name = strdup(name);
    q->machines = link_make();
    q->status = QUEUE_STAT_IDLE;
    link_enque(queues, q);

    return q;
}
