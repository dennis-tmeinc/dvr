#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

int main(void)
{
 struct sockaddr_un address;
 int  socket_fd, nbytes;
 char buffer[256];

 socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
 if(socket_fd < 0)
 {
  printf("socket() failed\n");
  return 1;
 }

 /* start with a clean address structure */
 memset(&address, 0, sizeof(struct sockaddr_un));

 address.sun_family = AF_UNIX;
 snprintf(address.sun_path, 512, "./demo_socket");

 if(connect(socket_fd,
            (struct sockaddr *) &address,
            sizeof(struct sockaddr_un)) != 0)
 {
  printf("connect() failed\n");
  return 1;
 }

 nbytes = snprintf(buffer, 256, "hello from a client");
 write(socket_fd, buffer, nbytes);

 nbytes = read(socket_fd, buffer, 256);
 buffer[nbytes] = 0;

 printf("MESSAGE FROM SERVER: %s\n", buffer);

 close(socket_fd);

 return 0;
}
