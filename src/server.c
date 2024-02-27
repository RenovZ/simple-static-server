#include "server.h"

void die(const char *s) {
  perror(s);
  exit(1);
}

void get_addr(char *addr, struct sockaddr *address) {
  if (address->sa_family == AF_INET) {
    struct sockaddr_in *sockaddr = (struct sockaddr_in *)address;
    sprintf(addr, "%s:%d", inet_ntoa(sockaddr->sin_addr),
            ntohs(sockaddr->sin_port));
  } else if (address->sa_family == AF_INET6) {
    struct sockaddr_in6 *sockaddr = (struct sockaddr_in6 *)address;
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(sockaddr->sin6_addr), ip, INET6_ADDRSTRLEN);
    sprintf(addr, "%s:%d", ip, ntohs(sockaddr->sin6_port));
  }
  return;
}

void set_socket(int *server_sock, u_short *port) {
  struct sockaddr_in sockaddr;
  *server_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (*server_sock == -1) {
    die("failed creating endpoint for communication");
  }

  int option_value = 1;
  setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &option_value,
             sizeof(option_value));

  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET; // ipv4
  sockaddr.sin_port = *port;
  sockaddr.sin_addr.s_addr = INADDR_ANY; // bind all avaiable interface

  if (bind(*server_sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
    die("failed binding a name to a socket");
  }

  if (*port == 0) {
    socklen_t socklen = sizeof(sockaddr);
    if (getsockname(*server_sock, (struct sockaddr *)&sockaddr, &socklen) ==
        -1) {
      die("failed getting socket name");
    }

    *port = sockaddr.sin_port;
  }

  if (listen(*server_sock, 5) < 0) {
    die("failed listenning connections on a socket");
  }

  return;
}

int main(void) {
  int server_sock = -1;
  u_short port = 8000;
  int client_sock = -1;
  struct sockaddr client_addr;
  socklen_t address_len = sizeof(client_addr);
  pthread_t idle_thread;
  set_socket(&server_sock, &port);

  printf("server started on port %d\n", port);
  while (1) {
    client_sock = accept(server_sock, &client_addr, &address_len);

    char addr[1024];
    get_addr(addr, &client_addr);
    printf("%s connected\n", addr);

    if (client_sock == -1)
      die("failed accepting request");

    if (pthread_create(&idle_thread, NULL, handle_request, &client_sock) != 0)
      perror("failed creating thread");
  }
  close(server_sock);

  return (0);
}
