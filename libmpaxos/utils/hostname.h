#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

static char* gethostip(const char* hostname) {
	struct hostent *h = NULL;
	h = gethostbyname(hostname);
	char *buf = malloc(100);
	h = gethostbyname(hostname);
	inet_ntop(h->h_addrtype, *h->h_addr_list, buf, 100);
	return buf;
}
