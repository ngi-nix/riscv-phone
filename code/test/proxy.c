#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "util.h"
#include "proxy.h"

ECPContext ctx;
ECPSocket sock;
ECPDHKey key_perma;

ECPNode node;
ECPConnection conn;

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <address> <node.priv> [node.pub]\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if ((argc < 3) || (argc > 4)) usage(argv[0]);
    
    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);
    
    rv = ecp_util_key_load(&ctx, &key_perma, argv[2]);
    printf("ecp_util_key_load RV:%d\n", rv);
    
    rv = ecp_sock_create(&sock, &ctx, &key_perma);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock, argv[1]);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);
    
    if (argc == 4) {
        rv = ecp_util_node_load(&ctx, &node, argv[3]);
        printf("ecp_util_node_load RV:%d\n", rv);

        rv = ecp_conn_create(&conn, &sock, ECP_CTYPE_PROXYB);
        printf("ecp_conn_create RV:%d\n", rv);

        rv = ecp_conn_open(&conn, &node);
        printf("ecp_conn_open RV:%d\n", rv);
    }

    while (1) sleep(1);
}