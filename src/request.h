#ifndef _REQUEST_H_
#define _REQUEST_H_

void *handle_request(void *clientsd);
int read_line(int sock, char *buf, int size);

#endif
