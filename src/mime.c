#include <string.h>
#include <strings.h>

#include "mime.h"

#define DEFAULT_MIME_TYPE "text/plain"

char *get_mime_type(const char *filename) {
  if (filename == NULL) {
    return DEFAULT_MIME_TYPE;
  }

  char *ext = strrchr(filename, '.');

  if (ext == NULL) {
    return DEFAULT_MIME_TYPE;
  }

  if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
    return "text/html";
  }
  if (strcasecmp(ext, "jpeg") == 0 || strcasecmp(ext, "jpg") == 0) {
    return "image/jpg";
  }
  if (strcasecmp(ext, "css") == 0) {
    return "text/css";
  }
  if (strcasecmp(ext, "js") == 0) {
    return "application/javascript";
  }
  if (strcasecmp(ext, "json") == 0) {
    return "application/json";
  }
  if (strcasecmp(ext, "txt") == 0) {
    return "text/plain";
  }
  if (strcasecmp(ext, "gif") == 0) {
    return "image/gif";
  }
  if (strcasecmp(ext, "png") == 0) {
    return "image/png";
  }

  return DEFAULT_MIME_TYPE;
}
