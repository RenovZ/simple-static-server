#ifndef _REQUEST_H_
#define _REQUEST_H_

typedef struct {
  int clientsd;
  char *user;
  char *password;
  char *path;
  char *addrstr;
} request_params;

void *handle_request(void *request_params);
int read_line(int sock, char *buf, int size);

#endif
