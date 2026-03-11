#include <string.h>
#include <strings.h>

#include "mime.h"

char *get_mime_type(const char *filename) {
  if (!filename)
    return DEFAULT_MIME_TYPE;

  char *ext = strrchr(filename, '.');
  if (!ext)
    return DEFAULT_MIME_TYPE;
  ext++;

  for (size_t i = 0; i < sizeof(mime_table) / sizeof(mime_table[0]); i++) {
    if (strcasecmp(ext, mime_table[i].ext) == 0)
      return (char *)mime_table[i].mime;
  }

  return DEFAULT_MIME_TYPE;
}
