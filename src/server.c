#include "server.h"
#include <stdio.h>
#include <sys/_types/_ssize_t.h>
#include <sys/socket.h>

void *handle_request(void *client_sock) {
  int sock = *(int *)client_sock;
  char buf[1024];
  int packets_len;
  // char method[255];
  // char url[1024];
  // char path[512];
  // size_t i, j;

  packets_len = read_line(sock, buf, sizeof(buf));
  printf("got %d chars from socket: %s\n", packets_len, buf);

  return NULL;
}

// parse a row of http packets
int read_line(int sock, char *buf, int size) {
  int i = 0;
  char c = '\0';
  int n;

  while (i < size - 1 && c != '\n') {
    n = recv(sock, &c, 1, 0);
    if (n > 0) {
      if (c == '\r') {
        n = recv(sock, &c, 1, MSG_PEEK);
        if (n > 0 && c == '\n') {
          recv(sock, &c, 1, 0);
        } else {
          c = '\n';
        }
      }
      buf[i] = c;
      i++;
    } else {
      c = '\n';
    }
  }
  buf[i] = '\0';
  return i;
}

void die(const char *s) {
  perror(s);
  exit(1);
}

void get_addr(char *addr, struct sockaddr *address) {
  if (address->sa_family == AF_INET) {
    struct sockaddr_in *sockaddr = (struct sockaddr_in *)address;
    sprintf(addr, "%s:%d", inet_ntoa(sockaddr->sin_addr),
            ntohs(sockaddr->sin_port));
  } else if (address->sa_family == AF_INET6) {
    struct sockaddr_in6 *sockaddr = (struct sockaddr_in6 *)address;
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(sockaddr->sin6_addr), ip, INET6_ADDRSTRLEN);
    sprintf(addr, "%s:%d", ip, ntohs(sockaddr->sin6_port));
  }
  return;
}

void startup(int *server_sock, u_short *port) {
  struct sockaddr_in addr;
  *server_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (*server_sock == -1) {
    die("failed creating endpoint for communication");
  }

  int option_value = 1;
  setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &option_value,
             sizeof(option_value));

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET; // ipv4
  addr.sin_port = htons(*port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // bind all available interface

  if (bind(*server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    die("failed binding a name to a socket");
  }

  if (*port == 0) {
    socklen_t addrlen = sizeof(addr);
    if (getsockname(*server_sock, (struct sockaddr *)&addr, &addrlen) == -1) {
      die("failed getting socket name");
    }

    *port = ntohs(addr.sin_port);
  }

  if (listen(*server_sock, 5) < 0) {
    die("failed listening connections on a socket");
  }

  return;
}

int main(void) {
  int server_sock = -1;
  u_short port = 8000;
  int client_sock = -1;
  struct sockaddr client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char s_addr[1024];
  pthread_t new_thread;

  startup(&server_sock, &port);

  printf("server started on port %d, with socket %d\n", port, server_sock);
  while (1) {
    client_sock = accept(server_sock, &client_addr, &client_addr_len);

    get_addr(s_addr, &client_addr);
    printf("%s connected\n", s_addr);

    if (client_sock == -1)
      die("failed accepting request");

    if (pthread_create(&new_thread, NULL, handle_request, &client_sock) != 0)
      perror("failed creating thread");
  }
  close(server_sock);
}
