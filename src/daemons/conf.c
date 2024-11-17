
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "res.h"

static struct resbd *init_resbd(const char *);
static struct params *init_params(const char *);
static link_t *init_queue(const char *, const char *);
static int handler(void *, const char *, const char *, const char *);

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
	struct resbd *r;
	r = init_resbd(value);
	if (r == NULL)
	    return 0;
	return 1;
    }

    if (strcmp(section, "params") == 0) {
	struct params *p = init_params(value);
	if (p == NULL)
	    return 0;
	return 1;
    }

    if (strstr(section, "queue")) {
	link_t *q = init_queue(section, value);
	if (q == NULL)
	    return 0;
	return 1;
    }

    return 1;
}

static struct resbd *
init_resbd(const char *val)
{
    return NULL;
}

static struct params *
init_params(const char *)
{
    return NULL;
}

static link_t *
init_queue(const char *, const char *)
{
    return NULL;
}
