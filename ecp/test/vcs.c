#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <core.h>
#include <vconn/vconn.h>

#include <util.h>

ECPContext ctx;
ECPSocket sock;
ECPConnection conn;

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <my addr> <my.priv> [ <node addr> <node.pub> ]\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    ECPDHKey key_perma;
    int rv;

    if ((argc < 3) || (argc > 5)) usage(argv[0]);

    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_util_load_key(&key_perma.public, &key_perma.private, argv[2]);
    printf("ecp_util_load_key RV:%d\n", rv);
    key_perma.valid = 1;

    rv = ecp_sock_create(&sock, &ctx, &key_perma);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock, argv[1]);
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    if (argc == 5) {
        ECPNode node;
        ecp_ecdh_public_t node_pub;

        rv = ecp_vlink_create(&conn, &sock);
        printf("ecp_vlink_create RV:%d\n", rv);

        rv = ecp_util_load_pub(&node_pub, argv[4]);
        printf("ecp_util_load_pub RV:%d\n", rv);

        rv = ecp_node_init(&node, &node_pub, argv[3]);
        printf("ecp_node_init RV:%d\n", rv);

        rv = ecp_conn_open(&conn, &node);
        printf("ecp_conn_open RV:%d\n", rv);
    }

    while (1) sleep(1);
}
