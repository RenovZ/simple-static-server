#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "misc.h"
#include "net.h"
#include "request.h"
#include "util.h"

void print_usage(char *name, u_short port) {
  printf("Usage: %s [options] [directory]\n", name);
  printf("Options:\n");
  printf("  --user <name>       Username\n");
  printf("  --password <pass>   Password\n");
  printf("  --host <host>       Host (default: localhost)\n");
  printf("  --port <port>       Port (default: %d, must be 1-65535)\n", port);
  printf("  [directory]         Target directory (default: .)\n");
}

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  setlocale(LC_ALL, "");

  int opt;
  char *user = NULL;
  char *password = NULL;
  char *host = "0.0.0.0";
  u_short port = 8000;
  char *directory = ".";

  static struct option long_options[] = {
      {"user", required_argument, 0, 'U'},
      {"password", required_argument, 0, 'P'},
      {"host", optional_argument, 0, 'H'},
      {"port", optional_argument, 0, 'p'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "u:p:H:P:h", long_options, NULL)) !=
         -1) {
    switch (opt) {
    case 'u':
      user = optarg;
      break;
    case 'p':
      password = optarg;
      break;
    case 'H':
      host = optarg;
      break;
    case 'P': {
      char *endptr;
      errno = 0;
      long val = strtoul(optarg, &endptr, 10);

      if (errno != 0 || *endptr != '\0' || val <= 0 || val > 65535) {
        fprintf(stderr, "Error: invalid port number '%s'\n", optarg);
        return 1;
      }
      port = val;
      break;
    }
    case 'h':
    default:
      print_usage(argv[0], port);
      return 0;
    }
  }

  if (optind < argc) {
    directory = argv[optind];
  }

  int serversd = -1;
  int clientsd = -1;
  struct sockaddr clientsa;
  socklen_t clientsa_len = sizeof(clientsa);
  char addrstr[BUFFER_SIZE];

  pthread_t newthread;

  bind_listener_sock(&serversd, host, &port);

  logmsg("%35s started on %s:%d, with socket %d", "", host, port, serversd);

  while (1) {
    clientsd = accept(serversd, &clientsa, &clientsa_len);

    get_in_addr(addrstr, &clientsa);

    request_params params = {.clientsd = clientsd,
                             .user = user,
                             .password = password,
                             .path = directory,
                             .addrstr = addrstr};

    if (clientsd == -1)
      die("failed accepting request");

    if (pthread_create(&newthread, NULL, handle_request, &params) != 0) {
      perror("failed creating thread");
    }
  }

  close(serversd);
}
