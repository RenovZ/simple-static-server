#ifndef _FILE_H_
#define _FILE_H_

void send_file(int clientsd, const char *filename);
void send_directory(int clientsd, const char *filepath);

#endif
