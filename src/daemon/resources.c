/* Handle request and return of resources
 */

#include "res.h"

void
check_queue_workload(void)
{
    link_t *l;
    struct queue *q;

    for (l = queues ->next; l != NULL; l = l->next) {
        q = (struct queue *)l->ptr;
        syslog(LOG_INFO, "%s: look at queue %s", __func__, q->name);
    }
}
