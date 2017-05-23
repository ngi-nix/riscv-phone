#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "util.h"

ECPContext ctx;
ECPSocket sock;
ECPDHKey key_perma;

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <node.priv> [address]\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if (argc != 2) usage(argv[0]);
    
    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);
    
    rv = ecp_util_key_load(&ctx, &key_perma, argv[1]);
    printf("ecp_util_key_load RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock, &ctx, &key_perma);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock, "0.0.0.0:3000");
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    while (1) sleep(1);
}