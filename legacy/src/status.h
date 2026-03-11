#ifndef _STATUS_H_
#define _STATUS_H_

#include <ctype.h>

typedef struct {
  int code;
  const char *text;
} status_t;

#define STATUS(code, text) {code, text}

#endif
