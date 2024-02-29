#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "misc.h"
#include "request.h"
#include "response.h"
#include "util.h"

void *handle_request(void *clientsd) {
  int sock = *(int *)clientsd;
  char buf[BUFFER_SIZE];
  size_t numchars;
  char method[255];
  char url[BUFFER_SIZE];
  char path[BUFFER_SIZE];
  size_t i, j;

  numchars = read_line(sock, buf, sizeof(buf));
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

// parse a row of http packets
int read_line(int sock, char *buf, int size) {
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
