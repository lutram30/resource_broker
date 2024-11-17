
#include "my_syslog.h"

static char logfile[PATH_MAX];
static char logident[10];
static int logmask;

static enum {
    LOGTO_SYS,
    LOGTO_FILE,
    LOGTO_STDERR
} log_dest;

static struct lastMsg *last;
#define REPEAT_TIMER 30;

static void verrlog(FILE *, const char *, va_list);
static void setLastMsg(const char *, struct timeval *);
static void printLastMsg(FILE *, struct timeval *);

/* verrlog()
 * vsnprintf() the client message.
 * Corrupt memory and mysteriously core dump if
 * the message is larger than BUFSIZ.
 */
static void
verrlog(FILE *fp, const char *fmt, va_list ap)
{
    int cc;
    int L;
    struct timeval now;
    static char buf[BUFSIZ];

    /* Initialiaze working variables
     */
    buf[0] = 0;
    L = 0;
    gettimeofday(&now, NULL);

    /* sprintf() on the buffer caller's data
     */
    cc = vsprintf(buf, fmt, ap);
    L += cc;
    sprintf(buf + L, "\n");

    if (last == NULL) {
        /* last is NULL if the client asked
         * the message to be sent to stderr,
         * in such case it is debugging and
         * it wants to see each and every message.
         */
        fprintf(fp, "\
%.15s.%06ld %d %s", ctime(&now.tv_sec) + 4,
                now.tv_usec, (int)getpid(), buf);
        return;
    }

    /* Compare the last message and the previous
     * if they are not equal print the current
     * and save it as last
     */
    if (strcmp(buf, last->msg) != 0) {

        printLastMsg(fp, &now);
        fprintf(fp, "\
%.15s.%d %d %s", ctime(&now.tv_sec) + 4,
                (int)now.tv_usec, (int)getpid(), buf);

        setLastMsg(buf, &now);

    } else { /* strcmp(buf, last->msg) == 0 */
        /* increase the counter how
         * many times we saw this message
         */
        last->count++;
        if (now.tv_sec > last->timeout) {
            /* if we saw the message for more
             * then REPEAT_TIMEOUT print how
             * many times we saw the last message
             */
            printLastMsg(fp, &now);
            setLastMsg(buf, &now);
        }
    } /* else */

    fflush(fp);

} /* verrlog() */

/* printLastMsg()
 */
static void
printLastMsg(FILE *fp, struct timeval *tv)
{
    /* A message that was seen only
     * once has counter 0
     */
    if (last->count == 0)
        return;

    fprintf(fp, "\
%.15s.%d %d ", ctime(&tv->tv_sec) + 4,
            (int)(tv->tv_usec/1000), (int)getpid());
    fprintf(fp, "\
 Last message repeated %d times\n", last->count);

} /* printLastMsg() */

/* setLastMsg()
 */
static void
setLastMsg(const char *buf,
           struct timeval *tv)
{
    memset(last->msg, 0, sizeof(last->msg));
    strcpy(last->msg, buf);
    last->count = 0;
    last->timeout = tv->tv_sec + REPEAT_TIMER;

} /* setLastMsg() */

void
openlog_(const char *ident,  /* name of the logger */
         const char *path,   /* where we should log */
         int maskpri,        /* da mask */
         int use_stderr)     /* stderr ? */
{
    /* save ident
     */
    strncpy(logident, ident, 9);
    logident[9] = '\0';

    logmask = maskpri;

    if (use_stderr) {
        log_dest = LOGTO_STDERR;
        return;
    }

    /* Make the last message data structure
     * the assumption is that if the caller
     * runs in -2 mode it wants to see all
     * messages even those repeated ones
     */
    last = calloc(1, sizeof(struct lastMsg));

    if (path && *path) {
        /* configuration specifies a logging directory
         */
        int lfp;

        /* Name, full path and suffiz log uniquely
         * identify the log file.
         */
        sprintf(logfile, "%s/%s.log", path, ident);

        if ((lfp = open(logfile, O_APPEND | O_CREAT)) == -1) {
            /* fall back to /tmp should you failed
             * to open the standard log file.
             */
            sprintf(logfile, "%s/%s.log", "/tmp", ident);
            lfp = open(logfile, O_APPEND | O_CREAT);
        }
        if (lfp != -1) {
            close(lfp);
            chmod(logfile, 0644);
            log_dest = LOGTO_FILE;
            return;
        }
    }
    /* we only get here if logging to a file isn't
     * configured or doesn't work
     */

    /* this is how pyx_syslog() knows where to go
     */
    log_dest = LOGTO_SYS;
    /* This is a bit tricky; always try the file first
     * if logfile[0] != '\0'
     */
    logfile[0] = '\0';

    /* Go to the system log syslog() da
     * mighty cousin...
     */
    openlog(ident, LOG_PID, LOG_DAEMON);
    setlogmask(logmask);

} /* openlog_() */

/* syslog_()
 */
void
syslog_(int level, const char *fmt, ...)
{
    va_list ap;
    static char buf[BUFSIZ];

    if (! (level <= logmask))
        return;

    va_start(ap, fmt);

    if (log_dest == LOGTO_STDERR) {
        verrlog(stderr, fmt, ap);
        return;
    }

    if (logfile[0]) {

        /* log to file
         */
        FILE   *lfp;
        if ((lfp = fopen(logfile, "a")) == NULL) {
            /* help! I used to be able to log to
             * the logfile...
             */
            if (log_dest == LOGTO_FILE) {
                log_dest = LOGTO_SYS;
                openlog(logident, LOG_PID, LOG_DAEMON);
                setlogmask(logmask);
            }
            goto use_syslog;
        }

        if (log_dest == LOGTO_SYS) {
            /* file has started working again */
            closelog();
            log_dest = LOGTO_FILE;
        }
        verrlog(lfp, fmt, ap);
        fclose(lfp);

        return;

    } /* if (logfile'0]) */

 use_syslog:
    vsprintf(buf + strlen(buf), fmt, ap);
    syslog(level, "%s", buf);

    va_end(ap);

}

/* pyx_closelog()
 */
void
closelog_(void)
{
    if (log_dest == LOGTO_SYS)
        closelog();
}

 /* setlogmask()
  */
int
setlogmask_(int maskpri)
{
    int oldmask = logmask;

    logmask = maskpri;

    oldmask = setlogmask(logmask);

    return(oldmask);
}
