#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include "status.h"

void serve_file(int clientsd, const char *dir, status_t *st);
void write_header(int clientsd, const char *key, const char *value);
void write_status(int clientsd, status_t *st);
void write_empty_line(int clientsd);

#endif
