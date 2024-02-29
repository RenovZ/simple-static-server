#ifndef _STATUS_H_
#define _STATUS_H_

#include <ctype.h>

typedef struct {
  int code;
  char *text;
  size_t len;
} status_t;

#define status(code, text)                                                     \
  { code, (char *)text, sizeof(text) - 1 }

#endif
