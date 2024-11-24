/* libresb client side
 */

#include "libresb.h"

/* Parse ip@port
 */
struct rb_daemon_id *
get_daemon_id(char *s)
{
    char *p;
    struct rb_daemon_id *d;

    d = calloc(1, sizeof(struct rb_daemon_id));

    p = strchr(s, '@');
    p = 0;
    d->ip = strdup(s);
    ++p;
    d->port = atoi(s);

    return d;
}

void
free_daemon_id(struct rb_daemon_id *d)
{
    free(d->ip);
    free(d);
}

struct rb_status *
rb_broker_status(struct rb_daemon_id *r)
{
    struct rb_message req;
    struct rb_message rep;
    struct rb_header *hdr;
    struct rb_header *hdr2;
    int cc;
    struct rb_status *s;

    hdr = calloc(1, sizeof(struct rb_header));
    hdr->opcode = BROKER_STATUS;
    hdr->len = 0;
    req.header = hdr;

    cc = nio_client_rw(r, &req, &rep);
    if (cc < 0) {
	free(hdr);
	return NULL;
    }

    hdr2 = rep.header;
    if (hdr2->opcode != BROKER_OK) {
	free(hdr);
	return NULL;
    }

    /* Decode and print
     */
    s = calloc(1, sizeof(struct rb_status));
    sscanf(rep.msg_buf, "%ld %d %lf %lf %lf", &s->uptime, &s->pid,
	   &s->load[0], &s->load[1], &s->load[2]);
    free(rep.msg_buf);

    return s;
}

char *
rb_broker_params(struct rb_daemon_id *r)
{
    struct rb_message req;
    struct rb_message rep;
    struct rb_header *hdr;
    struct rb_header *hdr2;
    int cc;

    hdr = calloc(1, sizeof(struct rb_header));
    hdr->opcode = BROKER_PARAMS;
    hdr->len = 0;
    req.header = hdr;

    cc = nio_client_rw(r, &req, &rep);
    if (cc < 0) {
	fprintf(stderr, "Network I/O error with the broker\n");
	free(hdr);
	return NULL;
    }

    hdr2 = rep.header;
    if (hdr2->opcode != BROKER_OK) {
	free(hdr);
	free(rep.header);
	return NULL;
    }
    free(hdr);
    free(rep.header);

    return rep.msg_buf;
}

char *
remote_addr(int s)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (getsockname(s, (struct sockaddr *)&addr, &addr_len) == -1) {
        syslog(LOG_ERR, "%s: getsockname failed %m", __func__);
	return NULL;
    }

    struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
    char ip[INET_ADDRSTRLEN];
    return (char *)inet_ntop(AF_INET, &(addr_in->sin_addr), ip, sizeof(ip));
}

char *
resolve_name(const char *name)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *p;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    /* Allow IPv4 or IPv6
     */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(name, NULL, &hints, &res);
    if (status != 0) {
	return NULL;
    }

    /* Loop through all the results
     */
    for (p = res; p != NULL; p = p->ai_next) {
        void *addr;

        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

	freeaddrinfo(res);
        return (char *)inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
    }

    return NULL;
}
