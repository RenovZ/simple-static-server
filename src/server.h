#ifndef _SIMPLE_HTTP_SERVER_H
#define _SIMPLE_HTTP_SERVER_H

#include <arpa/inet.h> // used for converting values between host and network byte order
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void *handle_request(void *client_sock);
void serve_file(int client_sock, const char *dir);
void write_response_header(int client_sock);
int read_request(int sock, char *buf, int size);
void die(const char *s);
void get_addr(char *addr, struct sockaddr *address);
void startup(int *server_sock, u_short *port);
void logmsg(const char *format, ...);

#endif
