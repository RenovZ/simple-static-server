#ifndef _NET_H_
#define _NET_H_

#include <sys/socket.h>

void get_in_addr(char *addr, struct sockaddr *sa);
void bind_listener_sock(int *serversd, const char *host, u_short *port);

#endif
