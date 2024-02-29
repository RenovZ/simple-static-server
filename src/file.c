#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "file.h"
#include "mime.h"
#include "misc.h"
#include "response.h"
#include "util.h"

const char *html_table_header =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <meta charset=\"utf-8\">"
    "  <style> a { text-decoration:none; } </style>"
    "</head>"
    "<body>"
    "  <div><a href=\"/\"><strong>[Root]</strong></a></div>"
    "  <hr />"
    "  <table>"
    "    <tr>"
    "      <th>Name</th>"
    "      <th>Last modified</th>"
    "      <th>Size</th>"
    "    </tr>"
    "    <tr><td style=\"border-top:1px dashed #BBB;\" "
    "colspan=\"5\"></td></tr>"
    "    <tr><td>&nbsp;</td></tr>";

const char *html_table_footer = "  </table>"
                                "</body>"
                                "</html>";

const char *html_file_seg = "    <tr>"
                            "      <td><a  href=\"%s%s\">%s</a></td>"
                            "      <td style=\"color:#888;\">[%s]</td>"
                            "      <td><bold>%s</bold></td>"
                            "    </tr>";

void send_file(int clientsd, const char *filename) {
  char buf[BUFFER_SIZE];
  ssize_t bytes_read;
  int fd;

  if ((fd = open(filename, O_RDONLY)) == -1) {
    die("failed opening dir");
  }

  write_header(clientsd, "Content-Type", get_mime_type(filename));
  write_empty_line(clientsd);

  while ((bytes_read = read(fd, buf, BUFFER_SIZE)) > 0) {
    send(clientsd, buf, bytes_read, 0);
  }

  close(fd);
}

void send_directory(int clientsd, const char *filepath) {
  struct dirent *entry;
  DIR *dir;
  char buf[BUFFER_SIZE];
  char path[2 * BUFFER_SIZE]; // fix: how to deal with long filename gracefully
  char entpath[BUFFER_SIZE];
  char filename[BUFFER_SIZE];
  unsigned long filepath_len;
  struct stat fstat;
  int errno;

  if ((filepath_len = strlen(filepath)) >= BUFFER_SIZE) {
    die("unsupported long filepath");
  }

  strncpy(path, filepath + 1, filepath_len);
  path[strlen(path)] = '\0';

  if (path[strlen(path) - 1] != '/') {
    path[strlen(path)] = '/';
    path[strlen(path) + 1] = '\0';
  }

  if ((dir = opendir(filepath)) == NULL) {
    die("opendir");
  }

  write_header(clientsd, "Content-Type", "text/html");
  write_empty_line(clientsd);

  send(clientsd, html_table_header, strlen(html_table_header), 0);

  while ((entry = readdir(dir)) != NULL) {
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
      continue;
    }

    sprintf(buf, "%s%s", path, entry->d_name);

    sprintf(filename, ".%s%s", path, entry->d_name);
    if ((errno = stat(filename, &fstat)) == -1) {
      sprintf(buf, "error: %s", strerror(errno));
      die(buf);
    }

    if (S_ISDIR(fstat.st_mode)) {
      sprintf(entpath, "%s/", entry->d_name);
    } else {
      strncpy(entpath, entry->d_name, strlen(entry->d_name));
      entpath[strlen(entry->d_name)] = '\0';
    }

    char *timestr = timefmt(&fstat.st_mtime, 0);
    char *sizestr = sizefmt(&fstat.st_size);
    sprintf(buf, html_file_seg, path, entpath, entpath, timestr, sizestr);
    free(timestr);
    free(sizestr);
    send(clientsd, buf, strlen(buf), 0);
  }

  send(clientsd, html_table_footer, strlen(html_table_footer), 0);

  closedir(dir);
}
