
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int raw_recv(void)
{
    int s;
    struct sockaddr_in saddr;
    char packet[50];

    if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror("error:");
        exit(EXIT_FAILURE);
    }

    memset(packet, 0, sizeof(packet));
    socklen_t *len = (socklen_t *)sizeof(saddr);
    int fromlen = sizeof(saddr);

    while(1) {
        if (recvfrom(s, (char *)&packet, sizeof(packet), 0,
            (struct sockaddr *)&saddr, &fromlen) < 0)
            perror("packet receive error:");

        int i = sizeof(struct iphdr);	/* print the payload */
        while (i < sizeof(packet)) {
            fprintf(stderr, "%c", packet[i]);
            i++;
        }
        printf("\n");
    }
    exit(EXIT_SUCCESS);
}


#define DEST "127.0.0.1"

int raw_send(void)
{

    int s;
    struct sockaddr_in daddr;
    char packet[50];
    /* point the iphdr to the beginning of the packet */
    struct iphdr *ip = (struct iphdr *)packet;

    if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror("error:");
        exit(EXIT_FAILURE);
    }

    daddr.sin_family = AF_INET;
    daddr.sin_port = 0; /* not needed in SOCK_RAW */
    inet_pton(AF_INET, DEST, (struct in_addr *)&daddr.sin_addr.s_addr);
    memset(daddr.sin_zero, 0, sizeof(daddr.sin_zero));
    memset(packet, 'A', sizeof(packet));   /* payload will be all As */

    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->tot_len = htons(40);	/* 16 byte value */
    ip->frag_off = 0;		/* no fragment */
    ip->ttl = 64;			/* default value */
    ip->protocol = IPPROTO_RAW;	/* protocol at L4 */
    ip->check = 0;			/* not needed in iphdr */
    ip->saddr = daddr.sin_addr.s_addr;
    ip->daddr = daddr.sin_addr.s_addr;

    while(1) {
        sleep(1);
        if (sendto(s, (char *)packet, sizeof(packet), 0,
            (struct sockaddr *)&daddr, (socklen_t)sizeof(daddr)) < 0)
            perror("packet send error:");
    }
    exit(EXIT_SUCCESS);
}
