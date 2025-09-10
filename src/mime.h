#ifndef _MIME_H_
#define _MIME_H_

#define DEFAULT_MIME_TYPE "text/plain"

typedef struct {
  const char *ext;
  const char *mime;
} mime_map_t;

static mime_map_t mime_table[] = {
    {"html", "text/html"},        {"htm", "text/html"},
    {"jpeg", "image/jpeg"},       {"jpg", "image/jpeg"},
    {"png", "image/png"},         {"gif", "image/gif"},
    {"css", "text/css"},          {"js", "application/javascript"},
    {"json", "application/json"}, {"txt", "text/plain"},
    {"mp3", "audio/mpeg"},        {"mp4", "video/mp4"},
    {"wav", "audio/wav"},         {"ogg", "audio/ogg"},
    {"pdf", "application/pdf"},
};

extern char *get_mime_type(const char *filename);

#endif
