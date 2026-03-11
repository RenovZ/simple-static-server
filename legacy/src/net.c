#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "misc.h"
#include "net.h"
#include "util.h"

void get_in_addr(char *buf, struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    struct sockaddr_in *sain = (struct sockaddr_in *)sa;
    sprintf(buf, "%s:%d", inet_ntoa(sain->sin_addr), ntohs(sain->sin_port));
  } else if (sa->sa_family == AF_INET6) {
    struct sockaddr_in6 *sain6 = (struct sockaddr_in6 *)sa;
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(sain6->sin6_addr), ip, INET6_ADDRSTRLEN);
    sprintf(buf, "%s:%d", ip, ntohs(sain6->sin6_port));
  }
  return;
}

void bind_listener_sock(int *serversd, const char *host, u_short *port) {
  struct sockaddr_in sain;
  int option_value = 1;

  *serversd = socket(PF_INET, SOCK_STREAM, 0);
  if (*serversd == -1) {
    die("failed creating endpoint for communication");
  }

  setsockopt(*serversd, SOL_SOCKET, SO_REUSEADDR, &option_value,
             sizeof(option_value));

  memset(&sain, 0, sizeof(sain));
  sain.sin_family = AF_INET; // ipv4
  sain.sin_port = htons(*port);

  if (host == NULL) {
    sain.sin_addr.s_addr = htonl(INADDR_ANY); // bind all available interface
  } else {
    if (inet_pton(AF_INET, host, &sain.sin_addr) <= 0) {
      die("invalid host IP");
    }
  }

  if (bind(*serversd, (struct sockaddr *)&sain, sizeof(sain)) < 0) {
    die("failed binding a name to a socket");
  }

  if (*port == 0) {
    socklen_t sain_len = sizeof(sain);
    if (getsockname(*serversd, (struct sockaddr *)&sain, &sain_len) == -1) {
      die("failed getting socket name");
    }

    *port = ntohs(sain.sin_port);
  }

  if (listen(*serversd, BACKLOG) < 0) {
    die("failed listening connections on a socket");
  }

  return;
}
