/* libresb client side
 */

#include "libresb.h"

int
get_server_data(struct rb_server *r)
{
    r->port = 41200;
    r->ip = "127.0.0.1";
    strcpy(r->name, "buntu24");

    return 0;
}

struct rb_status *
rb_broker_status(void)
{
    return NULL;
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
