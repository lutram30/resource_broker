/*
 * Logger library header file.
 */

#ifndef _SYSLOG_H_
#define _SYSLOG_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#ifdef LOG_ERR
#undef LOG_ERR
#endif
#define LOG_ERR  (1<<1)

#ifdef LOG_WARNING
#undef LOG_WARNING
#endif
#define LOG_WARNING  (1<<2)

#ifdef LOG_INFO
#undef LOG_INFO
#endif
#define LOG_INFO  (1<<3)

#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#define LOG_DEBUG  (1<<4)

/* yet another of many ways to
 * to implement the last message feature
 */
struct lastMsg {
    char msg[BUFSIZ];
    int count;
    time_t timeout;
};

extern void openlog_(const char *, const char *, int, int);
extern void syslog_(int, const char *, ...);
extern void closelog_(void);
extern int  setlogmas_k(int);

#endif /* _SYSLOG_H_ */
