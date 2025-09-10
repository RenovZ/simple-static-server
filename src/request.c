#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "misc.h"
#include "request.h"
#include "response.h"
#include "status.h"
#include "util.h"

void *handle_request(void *req_params) {
  request_params *params = (request_params *)req_params;
  int sock = params->clientsd;
  char buf[BUFFER_SIZE];
  size_t numchars;
  char method[255];
  char url[BUFFER_SIZE];
  char path[BUFFER_SIZE];
  char url_decoded[BUFFER_SIZE];
  char path_decoded[BUFFER_SIZE];
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

  if (params->path != NULL) {
    snprintf(path, sizeof(path), "%s%s", params->path, url);
  } else {
    snprintf(path, sizeof(path), ".%s", url);
  }

  decode_url(path_decoded, path);

  status_t st;
  serve_file(sock, path_decoded, &st);
  close(sock);

  decode_url(url_decoded, url);
  logmsg("%22s %d %8s %s", params->addrstr, st.code, method, url_decoded);

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
