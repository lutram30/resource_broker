/* nio interface
 */

/* Client part including service for the server like libc
 */
#include "libresb.h"

/* Server part
 */
#include "nio.h"

/* The idea behind this module is that the epoll file descriptor is private
 * to the library. The client/server communicate with each other using the
 * socket the libraryh returns.
 */
static int efd;

int
nio_server_init(int port)
{
    struct sockaddr_in addr;
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
    ev.data.fd = s;

    cc = epoll_ctl(efd, EPOLL_CTL_ADD, s, &ev);
    if (cc < 0) {
	close(s);
	return -1;
    }

    return s;
}

int
nio_client_init(const char *ip, int port)
{
    int s = nio_client_connect(port, ip);
    if (s < 0) {
	close(s);
	return -1;
    }

    efd = epoll_create(1024);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = s;

    int cc = epoll_ctl(efd, EPOLL_CTL_ADD, s, &ev);
    if (cc < 0) {
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
	return -1;
    }

    return nready;
}

int
nio_epoll2(struct epoll_event *events, int num_events, int ms)
{
    int nready;

    nready = epoll_wait(efd, events, num_events, ms);
    if (nready < 0) {
	return -1;
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
	return -1;
    }

    return 0;
}

int
nio_epoll_del(int s)
{
    if (epoll_ctl(efd, EPOLL_CTL_DEL, s, NULL) < 0) {
	return  -1;
    }

    return 0;
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

ssize_t
nio_client_rw(struct rb_daemon_id *r,
	      struct rb_message *req,
	      struct rb_message *rep)
{
    int s;
    ssize_t cc;
    struct rb_header *hdr;

    s = nio_client_connect(r->port, r->ip);
    if (s < 0) {
	close(s);
	return -1;
    }

    /* Write to client
     */
    cc = nio_writeblock(s, req->header, sizeof(struct rb_header));
    if (cc < 0) {
	close(s);
	return -1;
    }

    if (req->header->len > 0) {
	cc = nio_writeblock(s, req->msg_buf, req->header->len);
	if (cc < 0) {
	    close(s);
	    return -1;
	}
    }

    /* Read from client
     */
    hdr = calloc(1, sizeof(struct rb_header));
    cc = nio_readblock(s, hdr, sizeof(struct rb_header));
    if (cc < 0) {
	close(s);
	free(hdr);
	return -1;
    }

    rep->header = hdr;

    if (hdr->len == 0)
	return 0;

    rep->msg_buf = calloc(1, hdr->len);
    cc = nio_readblock(s, rep->msg_buf, hdr->len);
    if (cc < 0) {
	close(s);
	free(hdr);
	free(rep->msg_buf);
	return -1;
    }

    close(s);

    return 0;
}

int
nio_client_connect(int server_port, const char *server_addr)
{
    int s;
    int cc;
    int opt;
    struct sockaddr_in a;

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

    socklen_t addrlen = sizeof(struct sockaddr_in);
    cc = connect(s, (struct sockaddr *)&a, addrlen);
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
ssize_t
nio_readblock(int s, void *buf, size_t L)
{
    int c;
    size_t L2;

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
ssize_t
nio_writeblock(int s, const void *buf, size_t L)
{
    int c;
    size_t L2;

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
            return -1;
        }
    }

    return L;
}
