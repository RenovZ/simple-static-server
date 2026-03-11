#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "misc.h"
#include "request.h"
#include "response.h"
#include "status.h"
#include "util.h"

static int build_filesystem_path(char *dst, size_t dst_size, const char *root,
                                 const char *url) {
  char normalized[BUFFER_SIZE];
  size_t normalized_len = 0;
  const char *cursor = url;

  if (root == NULL || url == NULL || dst_size == 0 || url[0] != '/') {
    return -1;
  }

  normalized[normalized_len++] = '/';
  while (*cursor != '\0') {
    while (*cursor == '/') {
      cursor++;
    }

    if (*cursor == '\0') {
      break;
    }

    const char *segment_start = cursor;
    while (*cursor != '/' && *cursor != '\0') {
      cursor++;
    }

    size_t segment_len = cursor - segment_start;
    if (segment_len == 1 && strncmp(segment_start, ".", 1) == 0) {
      continue;
    }

    if (segment_len == 2 && strncmp(segment_start, "..", 2) == 0) {
      return -1;
    }

    if (normalized_len > 1) {
      normalized[normalized_len++] = '/';
    }

    if (normalized_len + segment_len >= sizeof(normalized)) {
      return -1;
    }

    memcpy(normalized + normalized_len, segment_start, segment_len);
    normalized_len += segment_len;
  }

  normalized[normalized_len] = '\0';
  if (snprintf(dst, dst_size, "%s%s", root, normalized) >= (int)dst_size) {
    return -1;
  }

  return 0;
}

void *handle_request(void *req_params) {
  request_params *params = (request_params *)req_params;
  int sock = params->clientsd;
  char buf[BUFFER_SIZE];
  size_t numchars;
  char method[255];
  char url[BUFFER_SIZE];
  char url_decoded[BUFFER_SIZE];
  char filepath[MAX_PATH];
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

  status_t st;
  decode_url(url_decoded, url);
  if (build_filesystem_path(filepath, sizeof(filepath), params->path,
                            url_decoded) != 0) {
    st.code = 403;
    st.text = "Forbidden";
    write_status(sock, &st);
    write_empty_line(sock);
  } else {
    serve_file(sock, filepath, url_decoded, &st);
  }
  close(sock);

  logmsg("%22s %d %8s %s", params->addrstr, st.code, method, url_decoded);

  free(params->addrstr);
  free(params);

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
