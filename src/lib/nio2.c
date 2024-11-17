
/* Network IO module. The core of the PYX relay system.
 */
#include "nio.h"

/* Relay sockets
 * The sockets are created idle and as clients requests
 * relay services they are activated and moved to the
 * activer queue. A client that gets shut is moved back
 * to the idle queue.
 */

/* Active I/O relays...
 */
static struct list_ *relaylist;
/* System controls
 */
static struct list_ *ctrlist;

/* Sockets in state RELAY_IDLE
 * are in this list
 */
static struct list_   *relaycache;

/* To display the status of active relays
 * in the system.
 */
static struct nio_relay **relayarray;

/* The global epoll socket descriptor
 */
static int efd;

/* controllers socket
 */
static struct list_  *ctrlcache;

/* Configuration parameters.
 */
static struct niodeclare *nd;

/* To display the status of active controls
 * in the system.
 */
static struct nio_ctrl **ctrlarray;

/* hashes peers waiting to call back to
 * start relay
 */
struct hashTab   *wpeers;

/* accept() a controller message or a peer message
 * asking us to start a relay.
 */
static struct nio_ctrl *nioacceptctrl(int, int);
static struct nio_ctrl *listener[2];
static int sslrelaystartread(struct nio_ctrl *);
static int niosslaccept(struct nio_ctrl *);
static int sslaccept(struct nio_ctrl *);
static void nioshutrelay(struct nio_relay *);
static void runrelays2(struct nio_relay *);
static inline int read_five_bytes(struct nio_ctrl *);
static int niolistensocket(int, in_addr_t);

/* Initializes the proxy networking layer.
 * Proxy uses this library for all its
 * networking i/o, the library is core
 * of the proxy.
 */
int
nioinit(struct niodeclare *nd2)
{
    int cc;
    int maxfds;

    /* Copy the address of the declare
     * structure since it is used all over
     * the daemon. Hopefully the caller does
     * not decide to free it.
     */
    nd = nd2;

    /* Make the global epoll control socket
     */
    efd = epoll_create(nd->maxrelay);
    if (efd < 0) {
        syslog_(LOG_ERR, "\
%s: epoll_create() failed %s", __func__, ERR);
        return -1;
    }

    listener[0] = calloc(1, sizeof(struct nio_ctrl));
    /* First thing, see if I can get control over
     * my listening socket, if I cannot no point
     * in doing anything else as I cannot run...
     */
    listener[0]->s = niolistensocket(nd->ctrlport,
                                     nd->pyx_ip);
    if (listener[0]->s < 0) {
        syslog_(LOG_ERR, "\
%s: failed %d get/bind/sockopt control listen socket...%s ",
                __func__, listener[0]->s, ERR);
        return -1;
    }
    listener[0]->ev = calloc(1, sizeof(struct epoll_event));
    /* No EPOLLET as we dont want edge-triggered
     * SSL already give us enough headache.
     */
    listener[0]->ev->events = EPOLLIN;
    listener[0]->ev->data.ptr = listener[0];

    cc = epoll_ctl(efd,
                   EPOLL_CTL_ADD,
                   listener[0]->s,
                   listener[0]->ev);
    if (cc < 0) {
        syslog_(LOG_ERR, "\
%s: epoll_ctl(%d) failed %s", __func__, listener[0]->s, ERR);
        return -1;
    }

    syslog_(LOG_INFO, "\
%s: root ctrl sock@port %d@%d nitialized ",
            __func__, listener[0]->s, nd->ctrlport);

    /* relay clients will connect to this
     * port sending their first protocol
     * message.
     * The dirty trick is that controller
     * come always on its own listening port
     * however the start relay message can come
     * as plain or SSL on the same port.
     * This is evil as we have to distinguish
     * which protocol is coming in.
     */
    listener[1] = calloc(1, sizeof(struct nio_ctrl));
    listener[1]->s = niolistensocket(nd->relayport,
                                     nd->pyx_ip);
    if (listener[1]->s < 0) {
        syslog_(LOG_ERR, "\
%s: failed %d get/bind/sock opt relay listen socket %s",
                __func__, listener[1]->s, ERR);
        return -1;
    }
    /* Set the first list socket in the
     * epoll tree.
     */
    listener[1]->ev = calloc(1, sizeof(struct epoll_event));
    listener[1]->ev->events = EPOLLIN;
    /* No EPOLLET as we dont want edge-triggered
     * SSL already give us enough headache with
     * that.
     */
    listener[1]->ev->data.ptr = listener[1];

    cc = epoll_ctl(efd,
                   EPOLL_CTL_ADD,
                   listener[1]->s,
                   listener[1]->ev);
    if (cc < 0) {
        syslog_(LOG_ERR, "\
%s: epoll_ctl(%d) failed %s", __func__, listener[1]->s, ERR);
        return -1;
    }

    syslog_(LOG_INFO, "\
%s: root relay sock@port %d@%d initialized",
            __func__, listener[1]->s, nd->relayport);

    /* Make caches and all working lists
     * Build the list from which we
     * pop connections we are going to accept.
     * We do this as we want to minimize the use
     * of dynamic memory, we don't want to have
     * to deal with trashing, memory bugs etc...
     */
    relaycache = listmake("rlay cache");
    relaylist = listmake("active relay");
    ctrlist = listmake("controllers");
    ctrlcache = listmake("control cache");

    maxfds = sysconf(_SC_OPEN_MAX);
    if (nd->maxsock > maxfds) {
        syslog_(LOG_ERR, "\
%s: maxfds %d maxsock %d pyxd can run out of fds...",
                __func__, maxfds, nd->maxsock);
        nd->maxsock = MIN(maxfds, nd->maxsock);
    }

    /* Initialize the list of relay data
     * structures.
     */
    for (cc = 0; cc < nd->maxrelay; cc++) {
        struct nio_relay   *r;

        r = calloc(1, sizeof(struct nio_relay));
        r->s = r->s = -1;
        r->status = RELAY_IDLE;
        r->sid = cc;
        r->buf = calloc(nd->relaybuf, sizeof(char));
        r->L = nd->relaybuf;
        r->outbuf = listmake("output buffers");
        r->sslmode = SSL_DO_OK;
        assert(r->buf);

        /* time to idle for connection in
         * RELAY_WAIT_ACCEPT state
         */
        r->ttidl = nd->ttidl;
        /* enqueue order by socket number
         */
        listenque(relaycache, (struct list_ *)r);
    }
    /* We insert the pointer to an active relay in this
     * array based on its sid so that pyxsockets can
     * read it and display it.
     */
    relayarray = calloc(nd->maxrelay, sizeof(struct nio_relay *));

    syslog_(LOG_INFO, "\
%s: %d relaycache socks initialized", __func__, relaycache->num);

    for (cc = 0; cc < nd->maxctrl; cc++) {
        struct nio_ctrl *ctrl;

        ctrl = calloc(1, sizeof(struct nio_ctrl));
        ctrl->s = -1;
        ctrl->snd = listmake("snd buf");
        ctrl->rcv = listmake("rcv buf");
        ctrl->index = cc;

        listenque(ctrlcache, (struct list_ *)ctrl);
    }
    ctrlarray = calloc(nd->maxctrl, sizeof(struct nio_ctrl *));

    syslog_(LOG_INFO, "\
%s: maxsock %d io sockets initialized", __func__, nd->maxsock);

    /* Build the hash table of controllers waiting
     * to become relay.
     */
    wpeers = hashmake(nd->maxrelay/10);

    return 0;

} /* nioinit() */

/* niomillisleep()
 */
void
niomillisleep(int msec)
{
    struct timeval tv;

    if (msec < 1)
        return;

    tv.tv_sec = 0;
    tv.tv_usec = msec * 1000;

    select(0, NULL, NULL, NULL, &tv);
}
/* Start a relay based on its sid
 * Morph the nio_ctrl to nio_relay...
 */
struct nio_relay *
niostartrelay(struct nio_ctrl *c,
              int sid)
{
    char key[128];
    struct nio_relay *r;

    sprintf(key, "%d", sid);
    r = hashrm(wpeers, key);
    if (r == NULL) {
        /* This can happen...
         * relay 1 is coming, connect() and
         * close() connection right away.
         * There are no pending data in the
         * buffer for the peer so niorelay
         * shuts down both ends and remove
         * the peer in RELAY_WAIT_ACCEPT
         * state from the hash table, if the
         * peer shows up later shut him down.
         * Or a relay is coming after it timed
         * out and we shut it down...
         */
        return NULL;
    }

    assert(r->status == RELAY_WAIT_ACCEPT);

    r->status = RELAY_ACTIVE;
    /* Get from cache and DO NOT
     * move to active list in active state.
     * Let the nioepoll move it to the
     * working list.
     *
     * IDLE --> WAIT --> ACTIVE --> DONE
     */
    listrm(relaycache, (struct list_ *)r);

    /* hop in the relay array.
     */
    relayarray[r->sid] = r;

    /* This is a way to morphe the control
     * structure into a relay one.
     */
    r->s = c->s;
    r->ev = c->ev;
    r->type = NIO_RELAY;
    gettimeofday(&r->start, NULL);

    memcpy(&r->from, &c->from, sizeof(struct sockaddr_in));

    syslog_(LOG_DEBUG, "\
%s: sid %d s %d status ACTIVE %d %s unblock %d",
            __func__, sid, r->s,
            r->status, niowho(&r->from),
            nioisunblock(r->s));

    /* Let nioepoll() via epoll_wait()
     * to keep the adddress of the
     * relay.
     */
    r->ev->data.ptr = r;

    /* Explicitly set the read bit
     * for epoll.
     */
    r->ev->events = EPOLLIN;
    r->bits = EPOLLIN;
    epoll_ctl(efd, EPOLL_CTL_MOD, r->s, r->ev);

    r->ssl = c->ssl;
    r->extra = c->extra;

    /* The control is shut bu the caller as
     * we replay PYXE_NO_ERROR to the client.
     */

    syslog_(LOG_DEBUG, "\
%s: ssl %s sid %d s %d status ACTIVE %d %s e 0x%x b 0x%x",
            __func__, (r->ssl ? "yes" : "no"), sid,
            r->s, r->status, niowho(&r->from),
            r->ev->events, r->bits);

    return r;

} /* niostartrelay() */

struct nio_peer *
niogetpeer(void)
{
    struct nio_relay         *r;
    struct nio_relay         *r1;
    struct nio_relay         *r2;
    static struct nio_peer   peer;

    r1 = r2 = NULL;

    /* Here is when we detect we ran out of
     * available session for relay...
     * As soon as relay finish we enqueue
     * them at the end of relaycache's list...
     */
    for (r = (struct nio_relay *)relaycache->forw;
         r != (void *)relaycache;
         r = r->forw) {

        if (r->status == RELAY_IDLE
            && r1 == NULL) {
            /* set the status, the buffer pointers,
             * timeout and what else is necessary
             * to start 2 relay
             */
            r->status = RELAY_WAIT_ACCEPT;
            r->x1 = r->x2 = 0;
            r->ttidl = nd->ttidl;
            r1 = r;
            peer.sid1 = r1->sid;
            continue;
        }

        if (r->status == RELAY_IDLE
            && r2 == NULL) {
            r->status = RELAY_WAIT_ACCEPT;
            r->x1 = r->x2 = 0;
            r->ttidl = nd->ttidl;
            r2 = r;
            peer.sid2 = r2->sid;
        }

        if (r2 && r1) {
            int dup;
            struct hash_ *h;
            char key[128];
            time_t t;

            /* I watch you and you
             * watch me...
             */
            r1->peer = r2;
            r2->peer = r1;
            /* record the time so we can track for
             * how long this relay is idle
             * and eventually release it.
             */
            t = time(NULL);
            r1->ttidl += t;
            r2->ttidl += t;

            sprintf(key, "%d", r1->sid);
            h = hashinstall(wpeers,
                            key,
                            (void *)r1,
                            &dup);
            assert(dup == 0);
            sprintf(key, "%d", r2->sid);
            h = hashinstall(wpeers,
                            key,
                            (void *)r2,
                            &dup);
            assert(dup == 0);

            syslog_(LOG_DEBUG, "\
niogetpeer: s %d status %d s %d status %d",
                        r1->sid, r1->status,
                        r2->sid, r2->status);

            return &peer;
        }
    } /* for (r = relaycache->back; ... ) */

    syslog_(LOG_ERR, "\
niogetpeer: ran out of relay sockets i %d a %d",
            LIST_NUM_ENTS(relaycache),
            LIST_NUM_ENTS(relaylist));

    return NULL;

} /* niogetpeer() */

/* Core operation...
 * Read from a writer socket and write to its peer reading
 * socket.
 */
void
niorelay(struct nio_relay *r)
{
    int cc;

    /* Shut already.
     */
    if (r->s < 0)
        return;

    /* Reset the events.
     */
    r->revents = 0;
    r->ev->events = 0;

    cc = niorelayread(r);
    if (cc <= 0) {
        /* eof or read error shutdown
         * peers
         */
        if (cc == 0
            || (errno != EWOULDBLOCK
                && errno != EAGAIN
                && errno != EINTR)) {
            if (cc < 0)
                niorelayerror("niorelay/niorelayread()", r);
            /* try to flush to r->peer
             */
            nioflush2peer(r);
            nioshutrelay(r);
            nioshutrelay(r->peer);
            return;
        }
    }

    if (r->peer->status == RELAY_ACTIVE) {

       cc = nioflush2peer(r);
        if (cc < 0) {
            /* write error, shutdown
             * peers
             */
            if (errno != EWOULDBLOCK
                && errno != EAGAIN
                && errno != EINTR) {
                niorelayerror("niorelay/nioflush2peer()", r);
                /* try to flush to r some data that
                 * r->peer might still have buffered.
                 */
                nioflush2peer(r->peer);
                nioshutrelay(r);
                nioshutrelay(r->peer);
                return;
            }
        }
    }
}

/* niorelayread()
 */
int
niorelayread(struct nio_relay *r)
{
    int cc;

    /* do not read 0 bytes
     * because read will return 0 and
     * yall dunno if eof arrived...
     */
    if (r->x1 == r->L)
        return 1;

    /*
     *      buf
     *   |------|----------|
     *          x1         L
     *      0 <= x1 <= L
     */
    errno = 0;
    cc = read(r->s,
              r->buf + r->x1,
              r->L - r->x1);
    if (cc <= 0) {
            syslog_(LOG_DEBUG, "\
niorelayread: cc %d x1 %d sid %d s %d read %dbytes %s",
                    cc, r->x1, r->sid, r->s, cc, ERR);
            return cc;
    }

    r->x1 = r->x1 + cc;
    r->rbytes += cc;

    syslog_(LOG_DEBUG, "\
niorelayread: sid %d s %d read %dbytes x1 %d peer sid %d",
            r->sid, r->s, cc, r->x1, r->peer->sid);

    return cc;

} /* niorelayread() */

/* nioflush2peer()
 */
int
nioflush2peer(struct nio_relay *r)
{
    int cc;

    /* This function will decide if we have to
     * set the pollout again. Even if we as r
     * get here before pyx processed the peer
     * it is ok if we reset the EPOLLOUT bit
     * because the function will eventually
     * reset it again. ev may be NULL if peer
     * not connected yet.
     */
    if (r->peer->ev == NULL)
        return 0;

    if (r->peer->bits & EPOLLOUT) {
        r->peer->ev->events = EPOLLIN;
        r->peer->bits = EPOLLIN;
        epoll_ctl(efd, EPOLL_CTL_MOD, r->peer->s, r->peer->ev);
        syslog_(LOG_DEBUG, "\
%s: event bits reset e 0x%x b 0x%x", __func__, r->peer->ev->events,
                r->peer->bits);
    }

    /* buffer already flushed
     */
    if (r->x1 == 0) {
        assert(r->x2 == 0);
        return 0;
    }
    assert(r->x1 != r->x2);

    /*     buf
     *   |----|---|-------|
     *       x2   x1      L
     *    0 <= x2 <= x1
     */
    errno = 0;
    cc = write(r->peer->s,
               r->buf + r->x2,
               r->x1 - r->x2);
    if (cc < 0) {
        /* errno is handled by the caller
         */
        syslog_(LOG_DEBUG, "\
%s: write got %d errno %s to peer %d from peer %d x1 %d x2 %d",
                __func__, cc, ERR,
                r->peer->sid, r->sid,
                r->x1, r->x2);
        /* Partial write so we must flush the buffer as soon
         * as the peer becomes writable again.
         */
        if (errno == EAGAIN
            || errno == EWOULDBLOCK) {

            r->peer->ev->events |= (EPOLLIN | EPOLLOUT);
            r->peer->bits |= (EPOLLIN | EPOLLOUT);
            epoll_ctl(efd, EPOLL_CTL_MOD, r->peer->s, r->peer->ev);
            syslog_(LOG_DEBUG, "\
%s: EPOLLOUT for sid %d s %d from %s flush asap e 0x%x b 0x%x",
                    __func__, r->peer->sid, r->peer->s,
                    niowho(&r->peer->from),
                    r->peer->ev->events, r->peer->bits);
            return -1;
        }
    }

    r->x2 = r->x2 + cc;
    r->wbytes += cc;

    syslog_(LOG_DEBUG, "\
%s: sid %d s %d wrote %dbytes x1 %d x2 %d partial write %d",
            __func__, r->peer->sid,
            r->peer->s, cc, r->x1, r->x2, r->x1 - r->x2);

    /* All written so reset the position
     * on the buffer. Alternatively
     * reset the buffer:
     * if (x1 == x2 && x1 == L)
     * however in this way we increase the
     * reading buffer by resetting its position
     * to the start.
     */
    if (r->x2 == r->x1) {
        r->x2 = r->x1 = 0;
    } else {
        /* Partial write so we must flush the buffer as soon
         * as the peer becomes writable again.
         */
        r->peer->ev->events |= (EPOLLIN | EPOLLOUT);
        r->peer->bits |= (EPOLLIN | EPOLLOUT);
        epoll_ctl(efd, EPOLL_CTL_MOD, r->peer->s, r->peer->ev);

        syslog_(LOG_DEBUG, "\
%s: EPOLLOUT for sid %d s %d from %s flush asap 0x%x 0x%x",
                __func__, r->peer->sid, r->peer->s,
                niowho(&r->peer->from),
                r->peer->ev->events, r->peer->bits);
    }

    return cc;

} /* nioflush2peer() */

static void
nioshutrelay(struct nio_relay *r)
{
    char key[32];
    struct nio_relay *r2;
    struct epoll_event ev;

    /* Shut already.
     */
    if (r->s < 0)
        return;

    /* In  kernel  versions  before  2.6.9, the EPOLL_CTL_DEL operation
     * required a non-NULL pointer in event, even though this  argument
     * is  ignored.   Since Linux 2.6.9, event can be specified as NULL
     * when using EPOLL_CTL_DEL.  Applications that need to be portable
     * to  kernels  before  2.6.9  should specify a non-NULL pointer in
     * event.
     */
    relayarray[r->sid] = NULL;

    /* Call back the main daemon to create
     * and log an accounting record.
     */
    (*nd->acct)(r);

    switch (r->status) {
        case RELAY_ACTIVE:
            syslog_(LOG_DEBUG, "\
%s: sid %d s %d from %s RELAY_ACTIVE", __func__,
                        r->sid, r->s, niowho(&r->from));
            epoll_ctl(efd, EPOLL_CTL_DEL, r->s, &ev);
            if (r->ev) {
                r->ev->data.ptr = NULL;
                freeup(r->ev);
            }
            if (r->ssl) {
                SSL_shutdown(r->ssl);
                SSL_free(r->ssl);
                r->ssl = NULL;
            }
            close(r->s);
            r->s = -1;
            r->revents = 0;
            r->status = RELAY_IDLE;
            r->x1 = r->x2 = 0;
            r->rbytes = r->wbytes = 0;
            r->t2l = 0;
            r->bits = 0;
            memset(&r->from, 0, sizeof(struct sockaddr_in));
            listenque(relaycache, (struct list_ *)r);
            return;

        case RELAY_WAIT_ACCEPT:
            syslog_(LOG_DEBUG, "\
%s: sid %d RELAY_WAIT_ACCEPT", __func__, r->sid);
            sprintf(key, "%d", r->sid);
            /* remember the peer is still in the
             * relaycache list.
             */
            r2 = hashrm(wpeers, key);
            assert(r2);
            assert(r2 == r);
            if (r->ssl) {
                SSL_shutdown(r->ssl);
                SSL_free(r->ssl);
                r->ssl = NULL;
            }
            r2->status = RELAY_IDLE;
            /* Remember a waiting relay
             * is still in the cache.
             */
    } /* switch() */

} /* nioshutrelay() */

/* nioshutctrl()
 * This function shuts the accepted controls either
 * because an error happened or because they morphed
 * into relays.
 */
void
nioshutctrl(struct nio_ctrl *c,
            int shut)
{
    struct nio_buf   *b;
    struct epoll_event ev;

    /* Already shut...
     */
    if (c->s == -1)
        return;

    /* if ctrl is morphed into relay
     * the relay inherits the socket
     * so don't close it ...
     */
    if (shut) {
        if (c->ssl) {
            SSL_shutdown(c->ssl);
            SSL_free(c->ssl);
            c->ssl = NULL;
        }
        epoll_ctl(efd, EPOLL_CTL_DEL, c->s, &ev);
        close(c->s);
        c->s = -1;
        freeup(c->ev);
    }
    /* inherited by the morphed relay
     */
    c->ev = NULL;
    c->ssl = NULL;

    if (c->snd) {
        while ((b = (struct nio_buf *)listpop(c->snd))) {
            free(b->buf);
            free(b);
        }
    }

    if (c->rcv) {
        while ((b = (struct nio_buf *)listpop(c->rcv))) {
            free(b->buf);
            free(b);
        }
    }
    c->accepting = 0;
    c->revents = 0;
    c->sslacceptstart = 0;
    /* Just in case someone is trying to
     * put a listening control structure
     * in the cache
     */
    if (c->forw && c->back)
        listenque(ctrlcache, (struct list_ *)c);

    ctrlarray[c->index] = NULL;

} /* nioshutctrl() */

/* nioreadblock()
 * We need this function because:
 * The system guarantees to read the number of bytes
 * requested if the descriptor references a normal file
 * that has that many bytes left before the end-of-file,
 * but in no other case.
 */
int
nioreadblock(int s,
             char *buf,
             int L)
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

} /* nioreadblock() */

/* write a socket in blocking mode
 */
int
niowriteblock(int s,
              char *buf,
              int L)
{
    int   c;
    int   L2;

    /* Make sure the socket is blocked
     */
    nioblock(s);

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

    return (L);

} /* niowriteblock() */

/* write to a sockect which is in non blocking
 * mode...
 */
int
niowritenonblock(int s,
                 char *buf,
                 int L)
{
    int   c;
    int   L2;

    L2 = L;
    while (L2 > 0) {
        c = write(s, buf, L);
        if (c > 0) {
            L2 -= c;
            buf += c;
        } else if (c < 0
                   && (errno != EINTR
                       && errno != EWOULDBLOCK
                       && errno != EAGAIN)) {
            return -1;
        }
    }

    return L;

} /* niowritenonblock() */


int
niosocktimer(int s, int ms)
{
    struct pollfd   p;
    unsigned int    n;
    int cc;

    p.fd = s;
    p.events = POLLIN;
    p.revents = 0;
    n = 1;

    while (1) {

        cc = poll(&p, n, ms);
        if (cc >= 0)
            return cc;

        if (errno == EINTR)
            continue;

        return -1;
    }

} /* niosocktimer() */

/* niotcpsocket()
 * Client tcp socket always bind to INADD_ANY.
 */
int
niotcpsocket(int port)
{
    int s;
    int cc;
    int opt;
    struct sockaddr_in   a;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    opt = 1;
    cc = setsockopt(s,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   (char *)&opt,
                   sizeof(opt));
    if (cc < 0) {
        close(s);
        return(-2);
    }

    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = INADDR_ANY; /* zero anyway */

    cc = bind(s,
              (struct sockaddr *)&a,
              sizeof(struct sockaddr_in));
    if (cc < 0) {
        close(s);
        return -1;
    }

    return s;

} /* niotcpsocket() */

/* Create a server listening socket on port port
 */
static int
niolistensocket(int port,
                in_addr_t addr)
{
    int s;
    int cc;
    int opt;
    struct sockaddr_in a;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    opt = 1;
    cc = setsockopt(s,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    (char *)&opt,
                    sizeof(opt));
    if (cc < 0) {
        close(s);
        return(-2);
    }

    if (nd->so_sndbuf > 0) {
        cc = setsockopt(s,
                        SOL_SOCKET,
                        SO_SNDBUF,
                        (int *)&nd->so_sndbuf,
                        sizeof(opt));
        if (cc < 0) {
            close(s);
            return -2;
        }
    }

    if (nd->so_rcvbuf > 0) {
        cc = setsockopt(s,
                        SOL_SOCKET,
                        SO_SNDBUF,
                        (int *)&nd->so_rcvbuf,
                        sizeof(opt));
        if (cc < 0) {
            close(s);
            return(-2);
        }
    }

    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = addr;
    a.sin_port = htons(port);

    cc = bind(s,
              (struct sockaddr *)&a,
              sizeof(struct sockaddr_in));
    if (cc < 0) {
        close(s);
        return -1;
    }

    cc = listen(s, SOMAXCONN);
    if (cc < 0) {
        close(s);
        return -3;
    }

    return s;

} /* niolistensocket() */

/* timeout unused for now...
 */
int
nioconnect(int s, struct sockaddr_in *a, int t)
{
    int   c;

    c = connect(s,
                (struct sockaddr *)a,
                sizeof(struct sockaddr));
    if (c < 0)
        return -1;

    return s;

} /* nioconnect() */

/* nioepoll()
 */
int
nioepoll(struct list_ **cl, struct list_ **rl)
{
    int cc;
    int i;
    int nready;
    static struct epoll_event *events;

    /* All communication queues
     * must have been drained by
     * the main daemon
     */
    assert(relaylist->forw == relaylist
           && relaylist->back == relaylist);

    if (events == NULL) {
        /* Prepare the array to which epoll_wait() is
         * going to copy the ready I/O data.
         */
        events = calloc(nd->maxrelay, sizeof(struct epoll_event));
    }

    nready = epoll_wait(efd,
                        events,
                        nd->maxrelay,
                        nd->polltimer);
    if (nready < 0) {
        syslog_(LOG_ERR, "\
%s: epoll_wait() failed %s", __func__, ERR);
    }

    for (i = 0; i < nready; i++) {
        struct epoll_event *e;
        struct nio_ctrl *ctrl;
        struct nio_relay *relay;
        struct common_ *common;
        int s;

        e = &(events[i]);

        /* Handle various I/O error
         */
        s = -1;
        /* Default ctrl listener
         */
        if (e->data.ptr == listener[0])
            s = listener[0]->s;
        /* Incoming relay listener it could
         * be either an ssl or non encrypted
         * incoming message.
         */
        if (e->data.ptr == listener[1])
            s = listener[1]->s;

        syslog_(LOG_DEBUG, "\
%s: nready %d events 0x%x s %d", __func__, nready, e->events, s);

        if (s > 0) {

            if ((e->events & EPOLLERR)
                || (e->events & EPOLLHUP)) {
                /* handle first an error on the
                 * accept socket
                 */
                syslog_(LOG_ERR, "\
%s: socket error on listener %d: %s", __func__, s, ERR);
                e->events = 0;
                continue;
            }
            /* Accept a new control
             */
            if (e->events & EPOLLIN) {
                /* accept() the new connection which
                 * can only be an ctrl data structure.
                 */
                ctrl = nioacceptctrl(s, 0);
                if (ctrl == NULL) {
                    syslog_(LOG_ERR, "\
%s: ohmygosh accept() on %d failed try next fd",
                            __func__, s);
                    continue;
                }

                /* If the incoming connect() is on the
                 * relay listener port try to accept
                 * ssl if we have the ssl object.
                 */
                if (e->data.ptr == listener[1])
                    niosslaccept(ctrl);
                continue;
            }

        } /* if (s > 0) */

        /* Find out if we dealing with a relay
         * or with a controller
         */
        ctrl = NULL;
        relay = NULL;
        common = e->data.ptr;
        if (common->type == NIO_CTRL) {
            ctrl = e->data.ptr;
            ctrl->revents = 0;
        } else {
            relay = e->data.ptr;
            relay->revents = 0;
        }

        syslog_(LOG_DEBUG, "\
%s: nready %d events 0x%x s %d type %d", __func__,
                nready, e->events, s, common->type);

        /* handle errors or EOF on existing
         * sockets
         */
        if ((e->events & EPOLLERR)
            || (e->events & EPOLLHUP)) {
            /* Error or EOF has happened so zero
             * out all possible events and let the
             * flow correct the error.
             */
            if (ctrl) {
                ctrl->revents = POLLERR;
                syslog_(LOG_DEBUG, "\
%s: POLLERR|POLLHUP %s sock %d shutdown control", __func__,
                        niowho(&ctrl->from), ctrl->s);
                nioshutctrl(ctrl, 1);
            }

            if (relay) {
                syslog_(LOG_DEBUG, "\
%s: relay POLLHUP|POLLERR shutdown sid %d s %d",
                        __func__, relay->sid, relay->s);
                /* Shut the couple relay and its peer.
                 */
                nioshutrelay(relay);
                nioshutrelay(relay->peer);
            }
            continue;
        }

        if (e->events & EPOLLIN) {

            if (ctrl) {

                if (ctrl->ssl == NULL) {
                    cc = nioasyncread(ctrl);
                    if (cc <= 0) {
                        /* Log this at LOG_DEBUG because
                         * we close the connection when
                         * a client goes away so we would
                         * se an error message even in a normal
                         * situation after the client leaves
                         * having obtained the sids.
                         */
                        syslog_(LOG_DEBUG, "\
%s: nioasyncread() %d shutdown control %s sock %d ssl %s",
                                __func__, cc,
                                niowho(&ctrl->from),
                                ctrl->s,
                                (ctrl->ssl ? "yes" : "no"));
                        nioshutctrl(ctrl, 1);
                    }
                    continue;
                }

                if (ctrl->accepting) {
                    /* Keep trying to accept
                     * ssl in non blocking mode.
                     */
                    niosslaccept(ctrl);
                    continue;
                }

                /* This must be ssl
                 */
                cc = sslrelaystartread(ctrl);
                if (cc <= 0) {
                    syslog_(LOG_ERR, "\
%s: sslnioasyncread() %d shutdown control %s sock %d ssl %s",
                            __func__, cc,
                            niowho(&ctrl->from),
                            ctrl->s,
                            (ctrl->ssl ? "yes" : "no"));
                    nioshutctrl(ctrl, 1);
                }
                continue;
            }

            if (relay) {
                syslog_(LOG_DEBUG, "\
%s: relay POLLIN sid %d s %d", __func__, relay->sid, relay->s);
                relay->revents |= POLLIN;
                runrelays2(relay);
                continue;
            }
        }

        if (e->events & EPOLLOUT) {

            if (relay) {

                /* check for spurious events
                 */
                if (relay->ssl == NULL) {
                    syslog_(LOG_DEBUG, "\
%s: zap relay sid %d s %d %s has EPOLLOUT 0x%x bits 0x%x",
                            __func__, relay->sid, relay->s,
                            niowho(&relay->from),
                            relay->ev->events, relay->bits);
                }
                relay->revents |= POLLOUT;
                runrelays2(relay);
                continue;
            }

            if (ctrl) {

                if (ctrl->ssl == NULL)
                    cc = nioasyncwrite(ctrl, 0);

                if (ctrl->ssl)
                    cc = sslnioasyncwrite(ctrl);
                if (cc <= 0) {
                    syslog_(LOG_DEBUG, "\
%s: asyncwrite/sslaccept() %d shutctrl %s sock %d ssl %s",
                            __func__, cc,
                            niowho(&ctrl->from),
                            ctrl->s, (ctrl->ssl ? "yes" : "no"));
                    nioshutctrl(ctrl, 1);
                    continue;
                }

                if (LIST_NUM_ENTS(ctrl->snd) == 0) {
                    /*
                     * No more things to write so reset
                     * EPOLLOUT.
                     */
                    ctrl->ev->events &= ~EPOLLOUT;
                }
                ctrl->ev->events |= EPOLLIN;
                epoll_ctl(efd, EPOLL_CTL_MOD, ctrl->s, ctrl->ev);
                continue;
            } /* if (ctrl) */

        } /* if (e->events & EPOLLOUT) { */

        syslog_(LOG_DEBUG, "\
%s unhandled event nready %d events 0x%x s %d type %d", __func__,
                nready, e->events, s, common->type);

    } /* for (i = 0; i < cc; i++) */

    /* The main daemon handles the control
     * operations.
     */
    *cl = ctrlist;

    return nready;
}

/* runrelays2()
 */
static void
runrelays2(struct nio_relay *r)
{
    gettimeofday(&r->last, NULL);

    if (r->ssl == NULL) {

        if (r->revents & POLLOUT) {
            syslog_(LOG_DEBUG, "\
%s: POLLOUT nioflush2peer() sid %d %d %s",
                    __func__, r->sid,
                    r->s, niowho(&r->from));
            /* pyx want me to be writable
             * which means my peer has some
             * data to relay to me
             */
            nioflush2peer(r->peer);
        }

        if (r->revents & POLLIN) {
            syslog_(LOG_DEBUG, "\
%s: POLLIN niorelay() sid %d %d %s",
                    __func__, r->sid,
                    r->s, niowho(&r->from));
            /* A pipe become readable so let's
             * relay data.
             */
            niorelay(r);
        }
        r->revents = 0;
        return;
    }

    if (r->ssl) {

        if (r->revents & POLLOUT) {
            syslog_(LOG_DEBUG, "\
%s: POLLOUT sid %d %d sslmode %d %s", __func__, r->sid, r->s,
                    r->sslmode, niowho(&r->from));
            /* Have to repeat previous SSL_read()
             * or SSL_write() operation because
             * SSL told us SSL_ERROR_WANT_WRITE
             */
            if (r->sslmode == SSL_DO_READ)
                sslniorelay(r);
            else if (r->sslmode == SSL_DO_WRITE)
                sslnioflush2peer(r);
            else
                abort();
        }

        if (r->revents & POLLIN) {
            syslog_(LOG_DEBUG, "\
%s: POLLIN sid %d %d sslmode %d %s", __func__, r->sid, r->s,
                    r->sslmode, niowho(&r->from));
            /* Have to repeat previous SSL_read()
             * or SSL_write() operation, because
             * SSL told us SSL_ERROR_WANT_READ,
             * or just an an SSL connection became
             * ready to relay.
             */
            if (r->sslmode == SSL_DO_READ)
                sslniorelay(r);
            else if (r->sslmode == SSL_DO_WRITE)
                sslnioflush2peer(r);
            else
                sslniorelay(r);
        }
    }
    r->revents = 0;

} /* runrelays2() */

/* nioacceptctrl()
 */
struct nio_ctrl *
nioacceptctrl(int sock, int port)
{
    socklen_t l;
    int s;
    int cc;
    struct nio_ctrl *ctrl;
    struct sockaddr_in from;

    ctrl = (struct nio_ctrl *)listpop(ctrlcache);
    if (ctrl == NULL) {
        syslog_(LOG_ERR, "\
%s: maxctrl %d reached, refusing accept()",
                __func__, nd->maxctrl);
        l = sizeof(struct sockaddr);
        s = accept(sock,
                   (struct sockaddr *)&from,
                   &l);
        close(s);
        return NULL;
    }

    /* just in case its left on from before
     * somehow...
     */
    ctrl->revents = 0;

    l = sizeof(struct sockaddr);
    ctrl->s = accept(sock,
                     (struct sockaddr *)&ctrl->from,
                     &l);
    if (ctrl->s < 0) {
        syslog_(LOG_ERR, "\
%s: accept(%d) failed %s", __func__, sock, ERR);
        listpush(ctrlcache, (struct list_ *)ctrl);
        return NULL;
    }
    niounblock(ctrl->s);

    ctrl->type = NIO_CTRL;
    ctrl->ev = calloc(1, sizeof(struct epoll_event));
    /* This is how we keep track of the
     * accepted control data structure,
     * we associate it with the socket
     * using the epoll_data void pointer.
     */
    ctrl->ev->data.ptr = ctrl;
    /* No EPOLLET as we dont want edge-triggered
     * SSL already give us enough headache with
     * that.
     */
    ctrl->ev->events = EPOLLIN;
    /* Here we add the accepted socket into the
     * epoll set of minitored file descriptors.
     */
    cc = epoll_ctl(efd, EPOLL_CTL_ADD, ctrl->s, ctrl->ev);
    if (cc < 0) {
        syslog_(LOG_ERR, "\
%s: epoll_ctl(%d) failed shutdown control %s", __func__, ctrl->s, ERR);
        nioshutctrl(ctrl, 1);
        return NULL;
    }

    /* Keep track of control for the
     * main daemon to display them.
     */
    ctrlarray[ctrl->index] = ctrl;

    syslog_(LOG_DEBUG, "\
%s: ctrl accepted new socket %d from %s", __func__,
                ctrl->s, niowho(&ctrl->from));
    return ctrl;

} /* nioacceptctrl() */

char *
niowho(const struct sockaddr_in *from)
{
    static char who[MAXHOSTNAMELEN];

    sprintf(who, "%d@%s", ntohs(from->sin_port),
            inet_ntoa(from->sin_addr));

    return(who);

} /* niowho() */

/* do async io on a accepted control
 * socket.
 */
int
nioasyncread(struct nio_ctrl *c)
{
    struct nio_buf *b;
    int cc;
    struct pyxheader hdr;

    /* even if this is a list the library expects
     * the caller to always process messages on
     * this channel one by one, only one active
     * messages is queued.
     */
    if (LIST_NUM_ENTS(c->rcv) == 0) {
        b = calloc(1, sizeof(struct nio_buf));
        assert(b);
        listpush(c->rcv, (struct list_ *)b);
    } else {
        b = (struct nio_buf *)c->rcv->back;
    }

    if (b->L == 0) {
        b->buf = calloc(sizeof(struct pyxheader), sizeof(char));
        assert(b->buf);
        b->p = 0;
        b->L = sizeof(struct pyxheader);
    }

    if (b->p == b->L) {
        c->revents = POLLIN;
        /* every time the main daemon processes
         * the ready I/O clients it must pop
         * them from the ctrl list.
         */
        listenque(ctrlist, (struct list_ *)c);
    }

    cc = read(c->s, b->buf + b->p, b->L - b->p);
    if (cc <= 0) {
        if (cc == 0)
            return 0;
        if (cc < 0
            && (errno != EINTR
                || errno != EWOULDBLOCK
                || errno != EAGAIN)) {
            return -1;
        }
        return b->p;
    }
    b->p += cc;

    if (b->L == sizeof(struct pyxheader)
        && b->p == b->L) {
        XDR   xdrs;

        xdrmem_create(&xdrs,
                      b->buf,
                      sizeof(struct pyxheader),
                      XDR_DECODE);

        if (!xdr_pyxheader(&xdrs, &hdr)) {
            syslog_(LOG_ERR, "\
%s: failed decode pyxheader from %s %m", __func__, niowho(&c->from));
            xdr_destroy(&xdrs);
            c->revents = POLLERR;
            return -1;
        }

        /* TODO here we should check if the
         * opCode is valid otherwise reject.
         */

        if (hdr.len) {
            b->L = sizeof(struct pyxheader) + hdr.len;
            b->buf = realloc(b->buf, b->L);
            assert(b->buf);
        }
        xdr_destroy(&xdrs);
    }

    if (b->p == b->L) {
        c->revents = POLLIN;
        /* every time the main daemon processes
         * the ready I/O clients it must pop
         * them from the ctrl list.
         */
        listenque(ctrlist, (struct list_ *)c);
    }

    return b->p;

} /* nioasyncread() */

/* write the buffer prepared by
 * niopreparewrite()
 */
int
nioasyncwrite(struct nio_ctrl *c,
              int writeall)
{
    struct nio_buf   *b;
    struct nio_buf   *b2;
    int              cc;
    int wrote;

    if (LIST_NUM_ENTS(c->snd) == 0)
        return 0;

    /* just address it for now
     */
    wrote = 0;
    b = (struct nio_buf *)c->snd->back;
    while (b != (void *)c->snd) {

        b2 = b->back;

        cc = write(c->s, b->buf + b->p, b->L - b->p);
        if (cc < 0) {
            if  (errno != EWOULDBLOCK
                 && errno != EAGAIN
                 && errno != EINTR) {
                c->revents = POLLERR;
            }
            return -1;
        }

        b->p += cc;
        wrote += cc;

        if (b->p == b->L) {
            /* ok pop it
             */
            b = (struct nio_buf *)listpop(c->snd);
            free(b->buf);
            free(b);
        }
        b = b2;

        /* in case multiple messages are
         * enqueued write only one...
         */
        if (writeall == 0)
            break;

    } /* while (b != c->snd->back) */

    return wrote;

} /* nioasyncwrite() */

/* take the user control buffer and prepare
 * it to be written by nioasyncwrite()
 * ubuf must be malloc() by the caller and
 * it is freed by nioasyncwrite()
 */
int
niopreparewrite(struct nio_ctrl *c,
                char *ubuf,
                int  size)
{
    struct nio_buf *b;
    int cc;

    c->ev->events |= (EPOLLIN | EPOLLOUT);
    cc = epoll_ctl(efd, EPOLL_CTL_MOD, c->s, c->ev);

    b = calloc(1, sizeof(struct nio_buf));
    assert(b);

    b->buf = ubuf;
    b->L = size;
    b->p = 0;

    listenque(c->snd, (struct list_ *)b);

    return 0;

} /* niopreparewrite() */

void
niorelayerror(const char *f,
              struct nio_relay *r)
{
    static char   buf[24];

    strcpy(buf, niowho(&r->from));

    syslog_(LOG_ERR, "\
niorelayerror: %s sid %d s %d stat %d %s peer sid %d s %d stat %d %s %m",
            f, r->sid, r->s, r->status, buf,
            r->peer->sid, r->peer->s, r->peer->status,
            niowho(&r->peer->from));

    if (r->ssl)
        essl("niorelayerror");

} /* niorelayerror() */

void
nioshutall(void)
{
    struct nio_ctrl   *c;
    struct nio_relay  *r;

    close(listener[0]->s);
    if (listener[1] != listener[0])
        close(listener[1]->s);

    while ((c = (struct nio_ctrl *)listpop(ctrlist))) {
        nioshutctrl(c, 1);
    }

    while ((r = (struct nio_relay *)listpop(relaylist))) {
        /* force shutdown...
         */
        r->status = RELAY_ACTIVE;
        nioshutrelay(r);
    }

} /* nioshutall() */

/* Find idle connections and shut them down if
 * their time to be idle expires
 */
void
niofindidle(time_t t)
{
    int z;
    struct hashwalk_ w;
    struct nio_relay *r;
    static struct nio_relay **u;

    if (HASH_NUM_HASHED(wpeers) == 0)
        return;

    if (!u) {
        u = calloc(nd->maxrelay,
                   sizeof(struct nio_relay *));
        assert(u);
    }

    /* traverse the hash table of peers whom
     * sid where given and which we are waiting
     * to start relay.
     */
    hashwalkstart(wpeers, &w);
    z = 0;

    while ((r = hashwalk(&w))) {

        syslog_(LOG_DEBUG, "\
%s: sid %d ttidl %d expire at %d", __func__,
                r->sid, nd->ttidl, r->ttidl);
        /* r->ttidl is an absolute time set when
         * the peers were sent out to the controller
         * in niogetpeer(). Now check if at this
         * time the relay is still disconnected
         * then terminate it.
         */
        if (t >= r->ttidl)
            u[z++] = r;
    }

    while ((--z) >= 0) {
        syslog_(LOG_DEBUG, "\
%s: shutdown sid %d ttidle %d expired %d",
                __func__, u[z]->sid,
                nd->ttidl, t);
        nioshutrelay(u[z]);
    }

} /* niofindidle() */

int
niounblock(int s)
{
    int   z;
    z = fcntl(s,  F_SETFL, O_NONBLOCK);
    return z;
} /* niounblock() */

int
nioblock(int s)
{
    int   z;
    z = fcntl(s, F_SETFL, fcntl(s, F_GETFL) & ~O_NONBLOCK);
    return z;

} /* nioblock() */

int
nioisunblock(int s)
{
    int cc;

    cc = fcntl(s, F_GETFL, 0);

    /* #define O_NONBLOCK 04000
     */
    return (cc & O_NONBLOCK);
}

struct nio_relay **
getrelayarray(void)
{
    return relayarray;
}

struct nio_ctrl **
getctrlarray(void)
{
    return ctrlarray;
}

/* Set blockin I/O parameters for an ssl connection.
 */
int
sslnioblock(SSL *ssl)
{
    int   z;

    z = SSL_get_fd(ssl);
    nioblock(z);

    if (0) {
        SSL_set_mode(ssl,
                     (SSL_get_mode(ssl) & ~SSL_MODE_ENABLE_PARTIAL_WRITE));
        SSL_set_mode(ssl,
                     (SSL_get_mode(ssl) & ~SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER));

        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    }

    return(z);

} /* niosslblock() */

/* Set non blocking parameters for a ssl connection
 */
int
sslniounblock(SSL *ssl)
{
    int   z;

    z = SSL_get_fd(ssl);
    niounblock(z);

    if (0) {
        SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
        SSL_set_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        SSL_set_mode(ssl,
                     (SSL_get_mode(ssl) & ~SSL_MODE_AUTO_RETRY));
    }

    return(z);

} /* sslniounblock() */

/* Perform the ssl handshake in non blocking
 * or blocking mode.
 */
int
niosslaccept(struct nio_ctrl *c)
{
    int cc;

    if (read_five_bytes(c))
        return 0;

    syslog_(LOG_DEBUG, "%s: running...", __func__);

    /* Set the ssl data structure in
     * the nio_ctrl structure.
     * Currently we expect this to be
     * the PYX_RELAY_START message,
     * this address will be passed
     * to nio_relay structure.
     */
    if (c->ssl == NULL) {
        /* Since we enter this routine potentially
         * multiple times we have to check if the
         * ssl isn't build already not to leak
         * memory or fuck up in any other way.
         */
        c->ssl = SSL_new(nd->ctx);
        if (!c->ssl) {
            essl(__func__);
            goto via;
        }

        cc = SSL_set_fd(c->ssl, c->s);
        if (cc == 0) {
            essl(__func__);
            goto via;
        }

        /* Let's try non blocking SSL_accept()
         * by default, otherwise block if
         * configured to do so.
         */
        if (nd->sslblockaccept)
            sslnioblock(c->ssl);
        else
            sslniounblock(c->ssl);
    }

    cc = sslaccept(c);
    if (cc < 0) {

        if (cc == -2) {
            syslog_(LOG_ERR, "\
%s: sslaccept() handshake timeout %d shutdown control %s sock %d ssl %s",
                    __func__, cc,
                    niowho(&c->from),
                    c->s,
                    (c->ssl ? "yes" : "no"));
            nioshutctrl(c, 1);
        }
        /* remember if we failed here does not
         * mean we failed an SSL connection it
         * could be just an ordinary accept()
         * but we have go to thru here to understand
         * if this is SSL or since we use the same port.
         */
        goto via;
    }

    sslniounblock(c->ssl);

    return 0;

via:
    if (c->ssl) {
        SSL_shutdown(c->ssl);
        SSL_free(c->ssl);
        c->ssl = NULL;
    }
    niounblock(c->s);

    return -1;

} /* sslaccept() */

/* read_five_bytes()
 */
static inline int
read_five_bytes(struct nio_ctrl *ctrl)
{
    char xbuf[5];

    if (nd->ctx)
        return 0;

    /* This routine kicks in only if the
     * PYX_NO_SSL is configured.
     * Since the client will send 5 bytes
     * as empty header to pyxd we have to
     * drain these 5 bytes otherwise the
     * pyx header will appear as corrupted.
     * Note we have to block to serialize with
     * the client since when we read their may
     * not be available bytes if the client
     * did not send them yet.
     */
    nioblock(ctrl->s);
    read(ctrl->s, xbuf, sizeof(xbuf));
    niounblock(ctrl->s);

    return 5;
}
/* sslaccept()
 * The core of ssl non blocking accept function.
 * sslaccept() can also return us
 * SSL_ERROR_WANT_READ || SSL_ERROR_WANT_WRITE
 * and set ctrl->accepting = 1;
 * In the first case nothing to do as we
 * turn on EPOLLIN no matter what, in the
 * second set EPOLLOUT.
 */
static int
sslaccept(struct nio_ctrl *ctrl)
{
    int cc;
    int z;
    time_t t;

    t = time(NULL);

    errno = 0;
    cc = SSL_accept(ctrl->ssl);
    z = SSL_get_error(ctrl->ssl, cc);
    switch (z) {

        case SSL_ERROR_NONE:

            syslog_(LOG_DEBUG, "\
%s: s %d SSL_accept() all right from %s", __func__,
                    SSL_get_fd(ctrl->ssl), niowho(&ctrl->from));
            /* done accepting...
             */
            ctrl->accepting = 0;
            /* The TLS/SSL handshake was successfully completed,
             * a TLS/SSL connection has been established.
             */
            return 1;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:

            /* No need to do anything as the
             * EPOLLIN bit is set already.
             */
            if (ctrl->accepting == 0)
                ctrl->sslacceptstart = t;
            ctrl->accepting++;

            syslog_(LOG_DEBUG, "\
%s: s %d SSL_accept() SSL_ERROR_WANT_READ|WRITE %s accepting %d at %d from %s.",
                    __func__, SSL_get_fd(ctrl->ssl),
                    ERR_error_string(cc, NULL),
                    ctrl->accepting,
                    ctrl->sslacceptstart,
                    niowho(&ctrl->from));

            /* Check if handshake timeout has expired.
             */
            if (t - ctrl->sslacceptstart >= nd->sslhndshktimeout) {
                syslog_(LOG_ERR, "\
%s: s %d SSL_accept() handshake timeout %d expired start %d now %d for %s",
                        __func__, SSL_get_fd(ctrl->ssl),
                        nd->sslhndshktimeout,
                        ctrl->sslacceptstart, t,
                        niowho(&ctrl->from));
                ctrl->accepting = 0;
                /* good bye for good.
                 */
                return -2;
            }
            /* Keep trying.
             */
            return 1;

        default:
            syslog_(LOG_DEBUG, "\
%s: s %d SSL_accept() z %d errno %d SSL_ERROR %s from %s", __func__,
                    SSL_get_fd(ctrl->ssl), z, errno,
                    ERR_error_string(cc, NULL),
                    niowho(&ctrl->from));
            ctrl->accepting = 0;
            essl(__func__);
            return -1;

    } /* switch(z) */

    abort();
}

/* just like nioasyncread() except that pyx protocol
 * is transported by the ssl protocol instead of
 * raw bloody tcp.
 */
int
sslnioasyncread(struct nio_ctrl *c)
{
    struct nio_buf *b;
    int cc;
    int z;
    struct pyxheader hdr;

    /* even if this is a list the library expects
     * the caller to always process messages on
     * this channel one by one, only one active
     * messages is queued.
     */
    if (LIST_NUM_ENTS(c->rcv) == 0) {
        b = calloc(1, sizeof(struct nio_buf));
        assert(b);
        listpush(c->rcv, (struct list_ *)b);
    } else {
        b = (struct nio_buf *)c->rcv->back;
    }

    if (b->L == 0) {
        b->buf = calloc(sizeof(struct pyxheader), sizeof(char));
        assert(b->buf);
        b->p = 0;
        b->L = sizeof(struct pyxheader);
    }

    if (b->p == b->L)
        c->revents = POLLIN;

    cc = SSL_read(c->ssl, b->buf + b->p, b->L - b->p);
    z = SSL_get_error(c->ssl, cc);
    switch (z) {
        case SSL_ERROR_NONE:
            b->p += cc;
            break;
        case SSL_ERROR_ZERO_RETURN:
            c->revents = POLLERR;
            break;
        case SSL_ERROR_WANT_READ:
            break;
        case SSL_ERROR_WANT_WRITE:
            break;
        default:
            c->revents = POLLERR;
            break;
    }

    if (b->L == sizeof(struct pyxheader)
        && b->p == b->L) {
        XDR   xdrs;

        xdrmem_create(&xdrs,
                      b->buf,
                      sizeof(struct pyxheader),
                      XDR_DECODE);

        if (!xdr_pyxheader(&xdrs, &hdr)) {
            syslog_(LOG_ERR, "\
%s: failed decode pyxheader from %s %m", __func__, niowho(&c->from));
            xdr_destroy(&xdrs);
            c->revents = POLLERR;
            return -1;
        }

        if (hdr.len) {
            b->L = sizeof(struct pyxheader) + hdr.len;
            b->buf = realloc(b->buf, b->L);
            assert(b->buf);
        }
        xdr_destroy(&xdrs);
    }

    if (b->p == b->L) {
        c->revents = POLLIN;
        /* every time the main daemon processes
         * the ready I/O clients it must pop
         * them from the ctrl list.
         */
        listenque(ctrlist, (struct list_ *)c);
    }

    return b->p;

} /* sslasyncread() */

/* sslrelaystartread()
 * The only ctrl message over SSL is relay
 * start so let's try to read it all in
 * one shot.
 */
int
sslrelaystartread(struct nio_ctrl *ctrl)
{
    int cc;

    do {
        cc = sslnioasyncread(ctrl);
        if (cc <= 0)
            return cc;
    } while (SSL_pending(ctrl->ssl) > 0);

    return cc;
}

int
sslnioasyncwrite(struct nio_ctrl *c2)
{
    struct nio_buf   *b;
    int              c;
    int              z;

    if (LIST_NUM_ENTS(c2->snd) == 0)
        return 0;

    /* just address it for now
     */
    b = (struct nio_buf *)c2->snd->back;

    c = SSL_write(c2->ssl,
                  b->buf + b->p,
                  b->L - b->p);
    z = SSL_get_error(c2->ssl, c);
    switch (z) {
        case SSL_ERROR_NONE:
            b->p += c;
            break;
        case SSL_ERROR_ZERO_RETURN:
            c2->revents = POLLERR;
            return -1;
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            break;
        default:
            c2->revents = POLLERR;
            return -1;
    }

    if (b->p == b->L) {
        /* ok pop it
         */
        b = (struct nio_buf *)listpop(c2->snd);
        free(b->buf);
        free(b);
    }

    return 0;

} /* sslasyncwrite() */

int
sslsyncwrite(SSL *ssl,
             char *buf,
             int L)
{
    int c0;
    int z;
    int L2;

    /* make sure the underlaying socket is
     * indeed blocked...
     */
    sslnioblock(ssl);

    L2 = L;
    while (L2 > 0) {

        c0 = SSL_write(ssl, buf, L);
        z = SSL_get_error(ssl, c0);
        switch (z) {
            case SSL_ERROR_NONE:
                buf = buf + c0;
                L2 = L2 - c0;
                break;
            default:
                essl(__func__);
                return -1;
        }
    }

    return L;

} /* sslsyncwrite() */

int
sslsyncread(SSL *ssl,
            char *buf,
            int L)
{
    int z;
    int c0;
    int L2;

    sslnioblock(ssl);

    L2 = 0;
    while (L2 < L) {
        c0 = SSL_read(ssl, buf, L);
        z = SSL_get_error(ssl, c0);
        switch (z) {
            case SSL_ERROR_NONE:
                L2 = L2 + c0;
                buf = buf + c0;
                break;
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                continue;
            default:
                essl(__func__);
                return -1;
        }
    }

    return L;

} /* sslsyncread() */

/* Relay core operation in ssl mode.
 */
int
sslniorelay(struct nio_relay *r)
{
    /* Already shut
     */
    if (r->s < 0)
        return 0;

    syslog_(LOG_DEBUG, "\
%s: processing sid %d s %d %s %d unblock %d", __func__,
            r->sid, r->s, niowho(&r->from),
            r->status, nioisunblock(r->s));

    /* read() from its SSL object
     * and enqueue into r->peer
     * outbound queue.
     */
    sslniorelayread(r);

    /* Now tell the peer to dequeue
     * its inbound data and send them
     * out using its SSL obejct.
     */
    if (r->peer->status == RELAY_ACTIVE)
        sslnioflush2peer(r->peer);

    return 0;
}

/* Send an empty SSL3 pyxheader.
 */
int
sslsndepyxheader(int s)
{
    char   e[SSL3_RT_HEADER_LENGTH];
    int    c;

    memset(e, 0, sizeof(e));

    c = niowriteblock(s, e, sizeof(e));

    return(c);

} /* sslsndpyxheadery() */

/* sslniorelayread()
 */
int
sslniorelayread(struct nio_relay *r)
{
    int cc;
    int z;
    static char who[36];
    struct nio_buf *nb;

    if (r->ssl == NULL)
        return -1;

    assert (r->x1 != r->L);
    strcpy(who, niowho(&r->peer->from));

    if (r->sslmode == SSL_DO_WRITE) {
        syslog_(LOG_DEBUG, "\
%s: s %d SSL_DO_WRITE sid %d x1 %d L %d from %s peer sid %d from %s",
                __func__, SSL_get_fd(r->ssl),
                r->sid, r->x1, r->L,
                niowho(&r->from),
                r->peer->sid, who);
        return 0;
    }

    /* If we are coming in as result of
     * an SSL_WANT_WRITE condition reset
     * the mask removing the EPOLLOUT
     * eventually SSL will ask for it
     * again.
     */
    if (r->sslmode == SSL_DO_READ) {
        r->ev->events = EPOLLIN;
        epoll_ctl(efd, EPOLL_CTL_MOD, r->s, r->ev);
    }

    r->sslmode = SSL_DO_OK;

    errno = 0;
    cc = SSL_read(r->ssl,
                  r->buf,
                  r->L);
    z = SSL_get_error(r->ssl, cc);
    switch (z) {
        case SSL_ERROR_NONE:
                syslog_(LOG_DEBUG, "\
%s: s %d SSL_read sid %d cc %d x1 %d L %d from %s peer sid %d from %s",
                        __func__, SSL_get_fd(r->ssl),
                        r->sid, cc, r->x1, r->L,
                        niowho(&r->from),
                        r->peer->sid, who);
            /* Allocate nio_buf for the
             * writer for anything we wrote.
             */
            nb = calloc(1, sizeof(struct nio_buf));
            nb->buf = calloc(cc, sizeof(char));
            nb->L = cc;
            nb->p = 0;
            memcpy(nb->buf, r->buf, cc);
            /* push the oldest at the end.
             */
            listenque(r->peer->outbuf, (struct list_ *)nb);
            /* update your position
             */
            r->x1 = 0;
            r->rbytes += cc;

            /* Reset the moving position x1 and zero
             * it out for pedantic reasons.
             */
            memset(r->buf, 0, r->L);

            break;
        case SSL_ERROR_WANT_READ:
            syslog_(LOG_DEBUG, "\
%s: SSL_ERROR_WANT_READ %s sid %d from %s peer sid %d from %s",
                    __func__,
                    ERR_error_string(cc, NULL),
                    r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
            /* No need to do anything as the
             * EPOLLIN bit is set already.
             */
            r->sslmode = SSL_DO_READ;
            break;
        case SSL_ERROR_WANT_WRITE:
            /* Set the epoll event and repeat the
             * above operation, since we did not
             * move along the buffer this must be true.
             * Remember the above operation failed.
             */
            syslog_(LOG_DEBUG, "\
%s: SSL_ERROR_WANT_WRITE %s sid %d from %s peer sid %d from %s",
                    __func__,
                    ERR_error_string(cc, NULL),
                    r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
            r->sslmode = SSL_DO_READ;
            break;
        case SSL_ERROR_SYSCALL:
            if (cc != 0) {
                /* Not an EOF from one of the peers.
                 */
                syslog_(LOG_ERR, "\
%s: SSL_ERROR_SYSCALL z %d cc %d errno %d %s x1 %d L %d sid %d from %s peer sid %d from %s",
                        __func__, z, cc, errno,
                        ERR_error_string(cc, NULL),
                        r->x1, r->L,
                        r->sid,
                        niowho(&r->from),
                        r->peer->sid, who);
            }
            sslnioflush(r);
            sslnioflush(r->peer);
            nioshutrelay(r);
            nioshutrelay(r->peer);
            return -1;
        case SSL_ERROR_SSL:
            /* Handle this strange SSL error by
             * simply trying again.
             */
            if (cc == EAGAIN
                || cc == EWOULDBLOCK) {
                syslog_(LOG_ERR, "\
%s: SSL_ERROR z %d cc %d errno %d %s x1 %d L %d sid %d from %s peer sid %d from %s",
                        __func__, z, cc, errno,
                        ERR_error_string(cc, NULL),
                        r->x1, r->L,
                        r->sid,
                        niowho(&r->from),
                        r->peer->sid, who);
                niorelayerror(__func__, r);
                syslog_(LOG_ERR, "\
%s: error SSL_ERROR_SSL keep going...", __func__);
                return 0;
            }
        default:
            syslog_(LOG_ERR, "\
%s: SSL_ERROR z %d cc %d errno %d %s x1 %d L %d sid %d from %s peer sid %d from %s",
                    __func__, z, cc, errno,
                    ERR_error_string(cc, NULL),
                    r->x1, r->L,
                    r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
            niorelayerror(__func__, r);
            /* Flush my queue to the peer and
             * peer's queue to me.
             */
            sslnioflush(r);
            sslnioflush(r->peer);
            nioshutrelay(r);
            nioshutrelay(r->peer);
            return -1;
    } /* switch(z) */

    return 0;

} /* sslniorelayread() */

/* sslnioflush2peer()
 */
int
sslnioflush2peer(struct nio_relay *r)
{
    int cc;
    int z;
    static char who[36];
    struct nio_buf *nb;

    if (r->ssl == NULL)
        return -1;

    if (r->sslmode == SSL_DO_READ) {
        syslog_(LOG_DEBUG, "\
%s: SSL_DO_READ sid %d x1 %d L %d from %s peer sid %d from %s",
                __func__, r->sid, r->x1, r->L,
                niowho(&r->from),
                r->peer->sid, who);
        return 0;
    }

    if (r->sslmode == SSL_DO_WRITE) {
        /* Now we are here to write either
         * because our mask popped EPOLLIN
         * or because our peer read and now
         * pushing the data to us.
         */
        r->ev->events = EPOLLIN;
        epoll_ctl(efd, EPOLL_CTL_MOD, r->s, r->ev);
    }

    r->sslmode = SSL_DO_OK;

    /* Let's set r to be the peer with
     * its own SSL object
     */
    if (LIST_NUM_ENTS(r->outbuf) == 0)
        return 0;

    strcpy(who, niowho(&r->peer->from));

    /* Point to the front buffer of the
     * queue but dont dequeue is so if
     * the write fails we retry exactly
     * the same operation.
     */
znovu:
    nb = (struct nio_buf *)r->outbuf->forw;
    if (LIST_NUM_ENTS(r->outbuf) == 0) {
        /* Ok the queue has been drained move on
         */
        return 0;
    }

    syslog_(LOG_DEBUG, "\
%s: s %d SSL_write to sid %d r->sid %d bytes pos %d", __func__,
            SSL_get_fd(r->ssl), r->sid,
            nb->L, nb->p);

    errno = 0;
    cc = SSL_write(r->ssl,
                   nb->buf + nb->p,
                   nb->L - nb->p);
    z = SSL_get_error(r->ssl, cc);
    switch (z) {
        case SSL_ERROR_NONE:

            nb->p = nb->p + cc;
            r->wbytes = r->wbytes + cc;

            syslog_(LOG_DEBUG, "\
%s: s %d SSL_write %d sid %d from %s to peer %d from %s",
                    __func__, SSL_get_fd(r->ssl),
                    cc, r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
            if (nb->p == nb->L) {
                /* dequeue the oldest
                 */
                nb = (struct nio_buf *)listdeque(r->outbuf);
                free(nb->buf);
                free(nb);
            }
            /* Go back and try to drain the queue
             * the reader can be slower than the
             * writer.
             */
            goto znovu;
            break;
        case SSL_ERROR_WANT_READ:
            syslog_(LOG_DEBUG, "\
%s: s %d SSL_ERROR_WANT_READ %s sid %d from %s peer sid %d from %s",
                    __func__, SSL_get_fd(r->ssl),
                    ERR_error_string(cc, NULL),
                    r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
            /* Wait for the descriptor to become
             * writeable and try again
             */
            r->sslmode = SSL_DO_WRITE;
            break;
        case SSL_ERROR_WANT_WRITE:
            syslog_(LOG_DEBUG, "\
%s: s %d SSL_ERROR_WANT_WRITE %s sid %d from %s peer sid %d from %s",
                    __func__, SSL_get_fd(r->ssl),
                    ERR_error_string(cc, NULL),
                    r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
            /* Wait for the descriptor to become
             * writeable and try again
             */
            r->sslmode = SSL_DO_WRITE;
            break;
        default:
            syslog_(LOG_ERR, "\
%s: SSL_ERROR z %d cc %d errno %d %s x1 %d L %d sid %d from %s peer sid %d from %s",
                    __func__, z, cc, errno,
                    ERR_error_string(cc, NULL),
                    r->x1, r->L,
                    r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
            niorelayerror(__func__, r);
            /* Try again..
             */
            if (z == SSL_ERROR_SSL) {
                syslog_(LOG_ERR, "\
%s: error SSL_ERROR_SSL keep going...", __func__);
                return 0;
            }
            /* Flush my queue to the peer and
             * peer's queue to me.
             */
            sslnioflush(r);
            sslnioflush(r->peer);
            nioshutrelay(r);
            nioshutrelay(r->peer);
            return -1;
    } /* switch(z) */

    if (r->sslmode == SSL_DO_WRITE) {

        /* SSL_WANT_WRITE so let's wait for
         * the EPOLLOUT event and write again.
         */
        r->ev->events = EPOLLOUT;
        epoll_ctl(efd, EPOLL_CTL_MOD, r->s, r->ev);

        syslog_(LOG_DEBUG, "\
%s: SSL_DO_WRITE EPOLLOUT on sid %d from %s peer sid %d from %s",
                    __func__,
                    r->sid,
                    niowho(&r->from),
                    r->peer->sid, who);
    }

    return 0;

} /* sslnioflush2peer() */

/* sslnioflush()
 * Try to flush the peer output queue
 * as much as you can before closing the
 * ssl object and the socket.
 */
void
sslnioflush(struct nio_relay *r)
{
    struct nio_buf *nb;
    int cc;
    int z;
    static char who[36];

    /* Relay not connected yet.
     */
    if (r->status == RELAY_WAIT_ACCEPT)
        return;

    strcpy(who, niowho(&r->peer->from));

    /* Flush r's queue to the peer.
     */
    while ((nb = (struct nio_buf *)listdeque(r->outbuf))) {

        cc = SSL_write(r->peer->ssl,
                       nb->buf + nb->p,
                       nb->L - nb->p);
        z = SSL_get_error(r->ssl, cc);
        switch (z) {
            case SSL_ERROR_NONE:

                nb->p = nb->p + cc;
                syslog_(LOG_DEBUG, "\
%s: SSL_write %d from sid %d from %s to peer sid %d from %s",
                        __func__,
                        cc, r->sid,
                        niowho(&r->from),
                        r->peer->sid, who);
                if (nb->p == nb->L) {
                    free(nb->buf);
                    free(nb);
                }
                break;
            default:
                /* too late to handle any error we
                 * are in error situation already.
                 */
                return;
        }

    } /* while () */

} /* sslnioflush() */
