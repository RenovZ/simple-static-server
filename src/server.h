#ifndef _SIMPLE_HTTP_SERVER_H
#define _SIMPLE_HTTP_SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void *handle_request(void *sock);
void die(const char *s);
void get_addr(char *addr, struct sockaddr *address);
void set_socket(int *server_sock, u_short *port);

#endif
