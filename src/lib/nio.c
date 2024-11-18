/* nio interface
 */

/* Client part including service for the server like libc
 */
#include "libresb.h"

/* Server part
 */
#include "nio.h"

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
nio_client_rw(struct rb_server *r, struct rb_request *req, struct rb_reply *rep)
{
    int s;
    int cc;

    s = nio_client_connect(r->port, r->ip);
    if (s < 0) {
	close(s);
	return -1;
    }

    cc = nio_writeblock(s, req->buf_request, req->len);
    if (s < 0) {
	close(s);
	return -1;
    }

    cc = nio_readblock(s, rep->buf_reply, rep->len);
    if (s < 0) {
	close(s);
	return -1;
    }

    return 0;
}

int
nio_client_connect(int server_port, const char *server_addr)
{
    int s;
    int cc;
    int opt;
    struct sockaddr_in   a;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    opt = 1;
    cc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    if (cc < 0) {
        close(s);
        return -2;
    }

    a.sin_family = AF_INET;
    a.sin_port = htons(server_port);
    a.sin_addr.s_addr = inet_addr(server_addr);

   cc = connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr));
   if (cc < 0) {
       close(s);
       return -1;
   }

    return s;
}

/* nio_readblock()
 * We need this function because:
 * The system guarantees to read the number of bytes
 * requested if the descriptor references a normal file
 * that has that many bytes left before the end-of-file,
 * but in no other case.
 */
int
nio_readblock(int s, char *buf, int L)
{
    int   c;
    int   L2;

    errno = 0;

    L2 = L;
    while (L > 0) {
        c = read(s, buf, L);
        if (c > 0) {
            L = L - c;
            buf += c;
        } else if (c == 0 || errno != EINTR) {
            if (c == 0)
                errno = ECONNRESET;
            return -1;
        }
    }

    return L2;
}

/* write a socket in blocking mode
 */
int
nio_writeblock(int s, char *buf, int L)
{
    int   c;
    int   L2;

    /* Make sure the socket is blocked
     */
    nio_block(s);

    L2 = L;
    while (L2 > 0) {

        c = write(s, buf, L);
        if (c > 0) {
            L2 -= c;
            buf += c;
        } else if (c < 0 && errno != EINTR) {
            return (-1);
        }
    }

    return L;
}
