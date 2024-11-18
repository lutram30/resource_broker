/*
 *
 * Elementary single linked list in C.
 */
#include"link.h"

/* Make a new link
 */
link_t *
link_make(void)
{
    link_t *link;

    link = calloc(1, sizeof(link_t));
    if (!link)
        return NULL;

    return link;
}

/* Insert in front of the head.
 * This is the stack push operation.
 */
int
link_push(link_t *head, void *val)
{
    link_t *el;

    if (!head)
        return -1;

    el = calloc(1, sizeof(struct link));
    if (!el)
        return -1;

    el->ptr = val;
    el->next = head->next;
    head->next = el;
    head->num++;

    return 0;
}

void
link_free(link_t * head)
{
    if (!head)
        return;

    while (link_pop(head))
        head->num--;

    free(head);
}


/* The stack pop operation.
 */
void *
link_pop(link_t * head)
{
    link_t *p;
    void *t;

    if (!head)
        return NULL;

    if (head->next != NULL) {
        p = head->next;
        head->next = p->next;
        t = p->ptr;
        free(p);
        head->num--;
        return t;
    }

    return NULL;
}

/* Queue operation. If the link is long
 * this operation is expensive.
 */
int
link_enque(link_t *head, void *val)
{
    link_t *p;
    link_t *p2;

    if (!head)
        return -1;

    p2 = calloc(1, sizeof(link_t));
    if (!p2)
        return -1;

    p2->ptr = val;

    for (p = head; p->next != NULL; p = p->next)
        ;

    p2->next = p->next;
    p->next = p2;
    head->num++;

    return 0;
}

/* The opposite of link_pop(), return
 * the first element in the list,
 * first in first out, if you inserted
 * the elements by link_push()
 */
void *
link_deque(link_t *head)
{
    link_t *p;
    link_t *p2;
    void *t;

    if (!head
        || !head->next)
        return NULL;

    p2 = head;
    p = head->next;

    while (p->next) {
        p2 = p;
        p = p->next;
    }

    p2->next = p->next;
    t = p->ptr;
    free(p);
    head->num--;

    return t;
}

void *
link_find(link_t *head, void *el)
{
    link_t *p;

    for (p = head->next;
	 p != NULL;
	 p = p->next) {
	if (p->ptr == el) {
	    return p->ptr;
	}
    }

    return NULL;
}

/* Remove the element val from the link.
 */
void *
link_rm(link_t *head, void *val)
{
    link_t *p;
    link_t *t;
    void *v;

    /* Since we have only one link
     * we need to keep track of 2
     * elements: the current and
     * the previous.
     */

    t = head;
    for (p = head->next;
         p != NULL;
         p = p->next) {
        if (p->ptr == val) {
            t->next = p->next;
            v = p->ptr;
            free(p);
            head->num--;
            return(v);
        }
        t = p;
    }

    return p;
}
