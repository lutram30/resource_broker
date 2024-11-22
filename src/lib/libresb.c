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
