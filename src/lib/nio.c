/* nio interface
 */

#include "libresb.h"

static int efd;

int
nio_server_init(int port)
{
    struct sockaddr_in addr;
    int sock;
    int cc;
    int opt;
    int s;

    efd = epoll_create(1024);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    opt = 1;
    cc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    if (cc < 0) {
        close(s);
        return(-2);
    }

    char *ip = "127.0.0.1";
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    cc = bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (cc < 0) {
        close(s);
        return -1;
    }

    cc = listen(s, SOMAXCONN);
    if (cc < 0) {
        close(s);
        return -3;
    }

    /* Kernel will return this data structure ones the fd is ready for io
     */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u64 = s;

    cc = epoll_ctl(efd, EPOLL_CTL_ADD, s, &ev);
    if (cc < 0) {
	syslog_(LOG_ERR, "%s: epoll_ctl(%d) failed %s", __func__, s, ERR);
	close(s);
	return -1;
    }

    return s;
}

/* nio_epoll()
 */
int
nio_epoll(struct epoll_event *events, int ms)
{
    int nready;

    nready = epoll_wait(efd, events, MAX_EVENTS, ms);
    if (nready < 0) {

    }

    return nready;
}

/* nio_millisleep()
 */
void
nio_millisleep(int msec)
{
    struct timeval tv;

    if (msec < 1)
        return;

    tv.tv_sec = 0;
    tv.tv_usec = msec * 1000;

    select(0, NULL, NULL, NULL, &tv);
}

int
nio_epoll_add(int s, struct epoll_event *ev)
{

    if (epoll_ctl(efd, EPOLL_CTL_ADD, s, ev) == -1) {
    }
}

void
nio_unblock(int s)
{
    fcntl(s,  F_SETFL, O_NONBLOCK);
}

void
nio_block(int s)
{
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) & ~O_NONBLOCK);
}

int
nio_client_io(int operation, struct op_request *, struct op_reply *)
{
    return 0;
}
