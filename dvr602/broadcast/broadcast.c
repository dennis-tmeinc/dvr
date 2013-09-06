#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
  struct in_addr ip, netmask, broadcast;

  if (argc < 3) {
    fprintf(stderr, "%s ipaddr netmask\n", argv[0]);
    exit(1);
  }

  if (!inet_aton(argv[1], &ip)) {
    fprintf(stderr, "Invalid address: %s\n", argv[1]);
    exit(1);
  }
  if (!inet_aton(argv[2], &netmask)) {
    fprintf(stderr, "Invalid address: %s\n", argv[2]);
    exit(1);
  }

  broadcast.s_addr = (ip.s_addr & netmask.s_addr) | ~netmask.s_addr;

  printf("%s", inet_ntoa(broadcast));

  return 0;
}
