/*
 *
 * Elementary single linked list in C.
 * D. Knuth Art of Computer Programming Volume 1. 2.2
 *
 */
#ifndef __LINK__
#define __LINK__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Each linked list is made of a head whose ptr is always
 * NULL, a list of following links starting from next and
 * a number num indicating how many elements are in the
 * list.
 */
typedef struct link {
    int num;
    void *ptr;
    struct link *next;
} link_t;

#define LINK_NUM_ENT(L) ((L)->num)

extern link_t *link_make(void);
extern void link_free(link_t *);
extern void *link_rm(link_t *, void *);
extern int link_push(link_t *, void *);
extern int link_enque(link_t *, void *);
extern void *link_deque(link_t *);
extern void *link_pop(link_t *);

#endif /* __LINK__ */
