/* Resource broker parsing configuration file
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "res.h"

static int init_resbd(const char *, const char *);
static int init_params(const char *, const char *);
static int init_queue(const char *, const char *);
static int handler(void *, const char *, const char *, const char *);
static struct queue *get_queue_by_name(const char *);
static int parse_machines(struct queue *, const char *);

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
    printf("section: %s key: %s value: %s\n", section, key, value);

    if (strcmp(section, "resbd") == 0) {
        return init_resbd(key, value);
    }

    if (strcmp(section, "params") == 0) {
        return init_params(key, value);
    }

    if (strstr(section, "queue")) {
        return init_queue(key, value);
    }

    return 1;
}

static int
init_resbd(const char *key, const char *val)
{
    if (strcmp(key, "host") == 0) {
        strcpy(res->host, val);
        return 1;
    }
    if (strcmp(key, "port") == 0) {
        res->port = atoi(val);
        return 1;
    }

    return 0;
}

static int
init_params(const char *key, const char *val)
{
    if (strcmp(key, "scheduler") == 0) {
        strcpy(prms->scheduler_type, val);
        return 0;
    }
    if (strcmp(key, "workload_timer") == 0) {
        prms->workload_timer = atoi(val);
        return 1;
    }

    return 0;
}

static int
init_queue(const char *key, const char *val)
{
    struct queue *q;

    q = get_queue_by_name(key);

    if (strcmp(key, "type") == 0) {
        if (strcmp(val, "vm") == 0)
            q->type = MACHINE_VMS;
        else
            q->type = MACHINE_CONTAINERS;
    }
    if (strcmp(key, "name_space") == 0) {
        strcpy(q->name_space, val);
    }
    if (strcmp(key, "borrow_factor") == 0) {
        sscanf(val, "%d%d", &q->borrow_factor[0], &q->borrow_factor[1]);
    }
    if (strcmp(key, "machines") == 0) {
        parse_machines(q, val);
    }
    return 0;
}

static struct queue *
get_queue_by_name(const char *name)
{
    struct queue *q;
    link_t *l;

    for (l = queues->next; l != NULL; l = l->next) {
        q = (struct queue *)l->ptr;
        if (strcmp(name, q->name) == 0) {
            return q;
        }
    }

    /* new queue
     */
    q = calloc(1, sizeof(struct queue));
    strcpy(q->name, name);
    q->machines = link_make();
    q->status = QUEUE_STAT_IDLE;

    return q;
}

static int
parse_machines(struct queue *q, const char *machines)
{
    char *token;
    char *s;
    char *str;
    char *str0;

    str0 = str = strdup(machines);

    token = strtok_r(str, ", ", &s);
    while (token) {
        struct machine *m;

        m = calloc(1, sizeof(struct machine));
        strcpy(m->name, token);
        m->status = MACHINE_IDLE;
        link_enque(q->machines, m);

        token = strtok_r(NULL, ", ", &s);
    }
    free(str0);

    return 1;
}
