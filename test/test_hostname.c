#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s hostname\n", argv[0]);
        exit(0);
    }

    char* hostname = argv[1];
    struct hostent *h = NULL;
    char buf[100];
    h = gethostbyname(hostname);
    if (!h) {
        printf("Couldn't lookup %s\n", hostname);
    } else {
        inet_ntop(h->h_addrtype, *h->h_addr_list, buf, 100);
        printf("Host for %s is %s\n", hostname, buf);
    }
    return 0;
}
