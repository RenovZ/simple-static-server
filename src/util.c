#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "misc.h"
#include "util.h"

// decode %xx in URL
void decode_url(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if (*src == '%' && isxdigit((unsigned char)src[1]) &&
        isxdigit((unsigned char)src[2])) {
      a = src[1];
      b = src[2];
      a = (a >= 'A') ? ((a & 0xDF) - 'A' + 10) : (a - '0');
      b = (b >= 'A') ? ((b & 0xDF) - 'A' + 10) : (b - '0');
      *dst++ = (char)(16 * a + b);
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

char *sizefmt(const long long *size) {
  char *sizestr = (char *)malloc(BUFFER_SIZE);

  memset(sizestr, 0, BUFFER_SIZE);

  if (*size >= 1024 * 1024 * 1024) {
    sprintf(sizestr, "%.2f GB", *size / (1024.0 * 1024 * 1024));
  } else if (*size >= 1024 * 1024) {
    sprintf(sizestr, "%.2f MB", *size / (1024.0 * 1024));
  } else if (*size >= 1024) {
    sprintf(sizestr, "%.2f KB", *size / 1024.0);
  } else {
    sprintf(sizestr, "%lld B", *size);
  }

  return sizestr;
}

char *timefmt(const time_t *tt, int with_usec) {
  struct timeval tv;
  struct tm *timeinfo;
  char *timestr = (char *)malloc(80);

  memset(timestr, 0, 80);

  if (tt == NULL) {
    gettimeofday(&tv, NULL);
    timeinfo = localtime(&tv.tv_sec);
  } else {
    timeinfo = localtime(tt);
  }
  strftime(timestr, 80, "%Y-%m-%d %H:%M:%S", timeinfo);

  if (tt == NULL) {
    if (with_usec == WITH_MILLSEC) {
      sprintf(timestr + strlen(timestr), ".%03d", tv.tv_usec / 1000);
    } else if (with_usec == WITH_MICROSEC) {
      sprintf(timestr + strlen(timestr), ".%06d", tv.tv_usec);
    } else if (with_usec == WITH_NANOSEC) {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      sprintf(timestr + strlen(timestr), ".%09ld", ts.tv_nsec);
    }
  }

  return timestr;
}

void die(const char *s) {
  perror(s);
  exit(1);
}

void logmsg(const char *format, ...) {
  va_list args;
  char *timestr = timefmt(NULL, WITH_MILLSEC);

  printf("%s\t", timestr);
  free(timestr);

  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  printf("\n");
}
