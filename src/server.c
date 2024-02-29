#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "misc.h"
#include "net.h"
#include "request.h"
#include "util.h"

int main(void) {
  int serversd = -1;
  u_short port = 8000;

  int clientsd = -1;
  struct sockaddr clientsa;
  socklen_t clientsa_len = sizeof(clientsa);
  char addrstr[BUFFER_SIZE];

  pthread_t newthread;

  bind_listener_sock(&serversd, &port);

  logmsg("      started on port %d, with socket %d", port, serversd);

  while (1) {
    clientsd = accept(serversd, &clientsa, &clientsa_len);

    get_in_addr(addrstr, &clientsa);
    // logmsg("      connected by %s", addrstr);

    if (clientsd == -1)
      die("failed accepting request");

    if (pthread_create(&newthread, NULL, handle_request, &clientsd) != 0)
      perror("failed creating thread");
  }

  close(serversd);
}
