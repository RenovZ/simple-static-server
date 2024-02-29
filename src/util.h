#ifndef _UTIL_H_
#define _UTIL_H_

#include <time.h>

#define WITH_MILLSEC 1
#define WITH_MICROSEC 2
#define WITH_NANOSEC 3

void die(const char *s);
void logmsg(const char *format, ...);
extern char *timefmt(const time_t *tt, int with_usec);
extern char *sizefmt(const long long *size);

#endif
