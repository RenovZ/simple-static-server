#include "server.h"
#include <string.h>
#include <sys/socket.h>

void *handle_request(void *client_sock) {
  int sock = *(int *)client_sock;
  char buf[BUFFER_SIZE];
  size_t numchars;
  char method[255];
  char url[BUFFER_SIZE];
  char path[BUFFER_SIZE];
  size_t i, j;

  numchars = read_request(sock, buf, sizeof(buf));
  i = 0, j = 0;
  while (!isspace(buf[i]) && i < sizeof(method) - 1) {
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';

  i = 0;
  while (isspace(buf[j]) && j < numchars) {
    j++;
  }
  while (!isspace(buf[j]) && i < sizeof(url) - 1 && j < numchars) {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';
  logmsg("[%s] %s", method, url);

  path[0] = '.';
  path[1] = '\0';
  sprintf(path + strlen(path), "%s", url);
  path[strlen(url) + 2] = '\0';
  serve_file(sock, path);

  close(sock);

  return NULL;
}

void serve_file(int client_sock, const char *dir) {
  char buf[BUFFER_SIZE];
  ssize_t bytes_read;
  int fd;

  printf("directory: [%s]\n", dir);
  if ((fd = open(dir, O_RDONLY)) == -1) {
    die("failed opening dir");
  }

  write_response_header(client_sock);

  while ((bytes_read = read(fd, buf, BUFFER_SIZE)) > 0) {
    send(client_sock, buf, bytes_read, 0);
  }
}

void write_response_header(int client_sock) {
  char buf[BUFFER_SIZE];

  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client_sock, buf, strlen(buf), 0);
  strcpy(buf, "Content-Type: text/html\r\n");
  send(client_sock, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client_sock, buf, strlen(buf), 0);
}

// parse a row of http packets
int read_request(int sock, char *buf, int size) {
  int i = 0;
  char c = '\0';
  ssize_t n;

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

void logmsg(const char *format, ...) {
  va_list args;
  struct timeval tv;
  struct tm *time_info;
  char time_str[80];

  gettimeofday(&tv, NULL);
  time_info = localtime(&tv.tv_sec);
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
  sprintf(time_str + strlen(time_str), ".%06d", tv.tv_usec);

  printf("%s\t", time_str);

  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  printf("\n");
}

int main(void) {
  int server_sock = -1;
  u_short port = 8000;
  int client_sock = -1;
  struct sockaddr client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char s_addr[BUFFER_SIZE];
  pthread_t new_thread;

  startup(&server_sock, &port);

  logmsg("      started on port %d, with socket %d", port, server_sock);
  while (1) {
    client_sock = accept(server_sock, &client_addr, &client_addr_len);

    get_addr(s_addr, &client_addr);
    logmsg("      connected by %s", s_addr);

    if (client_sock == -1)
      die("failed accepting request");

    if (pthread_create(&new_thread, NULL, handle_request, &client_sock) != 0)
      perror("failed creating thread");
  }
  close(server_sock);
}
