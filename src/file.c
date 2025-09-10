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

void send_file(int clientsd, const char *filepath) {
  int fd = open(filepath, O_RDONLY);
  if (fd < 0)
    return; // file not found, ignore

  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    return;
  }

  char buf[BUFFER_SIZE];
  char sizebuf[32];

  write_header(clientsd, "Content-Type", get_mime_type(filepath));
  snprintf(sizebuf, sizeof(sizebuf), "%lld", st.st_size);
  write_header(clientsd, "Content-Length", sizebuf);
  write_empty_line(clientsd);

  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    ssize_t total_sent = 0;
    while (total_sent < n) {
      ssize_t sent = send(clientsd, buf + total_sent, n - total_sent, 0);
      if (sent < 0) {
        if (errno == EINTR)
          continue;
        perror("send failed");
        close(fd);
        return;
      }
      total_sent += sent;
    }
  }

  if (n < 0) {
    perror("read failed");
  }

  close(fd);
}

const char *html_header =
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

const char *html_footer = "  </table>"
                          "</body>"
                          "</html>";

const char *html_filetpl = "    <tr>"
                           "      <td><a  href=\"%s%s\">%s</a></td>"
                           "      <td style=\"color:#888;\">[%s]</td>"
                           "      <td><bold>%s</bold></td>"
                           "    </tr>";

void send_directory(int clientsd, const char *dirpath) {
  struct dirent *entry;
  DIR *dir;
  char buf[BUFFER_SIZE];
  char path[2 * BUFFER_SIZE]; // fix: how to deal with long filename gracefully
  char entpath[BUFFER_SIZE];
  char filename[BUFFER_SIZE];
  unsigned long filepath_len;
  struct stat fstat;
  int errno;

  if ((filepath_len = strlen(dirpath)) >= BUFFER_SIZE) {
    die("unsupported long filepath");
  }

  strncpy(path, dirpath + 1, filepath_len);
  path[strlen(path)] = '\0';

  if (path[strlen(path) - 1] != '/') {
    path[strlen(path)] = '/';
    path[strlen(path) + 1] = '\0';
  }

  if ((dir = opendir(dirpath)) == NULL) {
    die("opendir");
  }

  write_header(clientsd, "Content-Type", "text/html");
  write_empty_line(clientsd);

  send(clientsd, html_header, strlen(html_header), 0);

  while ((entry = readdir(dir)) != NULL) {
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
      continue;
    }

    snprintf(buf, sizeof(buf), "%s%s", path, entry->d_name);

    snprintf(filename, sizeof(filename), ".%s%s", path, entry->d_name);
    if ((errno = stat(filename, &fstat)) == -1) {
      snprintf(buf, sizeof(buf), "error: %s", strerror(errno));
      die(buf);
    }

    if (S_ISDIR(fstat.st_mode)) {
      snprintf(entpath, sizeof(entpath), "%s/", entry->d_name);
    } else {
      snprintf(entpath, sizeof(entpath), "%s", entry->d_name);
    }

    char *timestr = timefmt(&fstat.st_mtime, 0);
    char *sizestr = sizefmt(&fstat.st_size);
    snprintf(buf, sizeof(buf), html_filetpl, path, entpath, entpath, timestr,
             sizestr);
    free(timestr);
    free(sizestr);
    send(clientsd, buf, strlen(buf), 0);
  }

  send(clientsd, html_footer, strlen(html_footer), 0);
  closedir(dir);
}
