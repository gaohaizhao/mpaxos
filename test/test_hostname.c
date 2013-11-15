#include <utils/hostname.h>
#include <check.h>

START_TEST(hostname) {
    char *ip = NULL;

    char *host = "www.google.com";
    char *host_na = "www.imustnotexist.com";

    ip = gethostip(host);
    printf("%s:%s\n", host, ip);
    free(ip);

    ip = gethostip(host_na);
    ck_assert(ip == NULL);
    if (ip) free(ip);

} END_TEST

//int main(int argc, char** argv) {
//    if (argc < 2) {
//        printf("Usage: %s hostname\n", argv[0]);
//        exit(0);
//    }
//
//    char* hostname = argv[1];
//    struct hostent *h = NULL;
//    char buf[100];
//    h = gethostbyname(hostname);
//    if (!h) {
//        printf("Couldn't lookup %s\n", hostname);
//    } else {
//        inet_ntop(h->h_addrtype, *h->h_addr_list, buf, 100);
//        printf("Host for %s is %s\n", hostname, buf);
//    }
//    return 0;
//}
