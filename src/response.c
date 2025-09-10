#include <fcntl.h> //file control
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file.h"
#include "misc.h"
#include "request.h"
#include "response.h"
#include "status.h"

void serve_file(int clientsd, const char *filepath, status_t *st) {
  char buf[BUFFER_SIZE];
  ssize_t bytes_read;
  struct stat fileinfo;

  while ((bytes_read = read_line(clientsd, buf, sizeof(buf))) > 0 &&
         strcmp(buf, "\n"))
    ;

  if (stat(filepath, &fileinfo)) {
    st->code = 404;
    st->text = "Not Found";
    write_status(clientsd, st);
    return;
  }

  st->code = 200;
  st->text = "OK";
  write_status(clientsd, st);

  if (S_ISDIR(fileinfo.st_mode)) {
    send_directory(clientsd, filepath);
  } else {
    send_file(clientsd, filepath);
  }
}

void write_status(int clientsd, status_t *st) {
  char buf[BUFFER_SIZE];

  sprintf(buf, "HTTP/1.0 %d %s\r\n", st->code, st->text);
  send(clientsd, buf, strlen(buf), 0);
}

void write_header(int clientsd, const char *key, const char *value) {
  char buf[BUFFER_SIZE];

  // fix: other headers
  sprintf(buf, "%s: %s\r\n", key, value);
  send(clientsd, buf, strlen(buf), 0);
}

void write_empty_line(int clientsd) {
  char buf[BUFFER_SIZE];

  strcpy(buf, "\r\n");
  send(clientsd, buf, strlen(buf), 0);
}
