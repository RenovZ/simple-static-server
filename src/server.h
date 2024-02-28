#ifndef _SIMPLE_HTTP_SERVER_H
#define _SIMPLE_HTTP_SERVER_H

#include <arpa/inet.h> // used for converting values between host and network byte order
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void *handle_request(void *client_sock);
int read_line(int sock, char *buf, int size);
void die(const char *s);
void get_addr(char *addr, struct sockaddr *address);
void startup(int *server_sock, u_short *port);

#endif
