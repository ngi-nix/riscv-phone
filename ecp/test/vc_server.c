#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <core.h>
#include <vconn/vconn.h>

#include <util.h>

ECPContext ctx;
ECPSocket sock;
ECPConnHandler handler;
ECPConnection conn;
ECPConnection vconn[3];

#define CTYPE_TEST  0
#define MTYPE_MSG   0

static int handle_open(ECPConnection *conn, ECP2Buffer *b) {
    printf("OPEN\n");

    return ECP_OK;
}

static ssize_t handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    char *_msg = "VAISTINU JE CAR!";
    ssize_t rv;

    printf("MSG:%s size:%ld\n", msg, msg_size);
    rv = ecp_msg_send(conn, MTYPE_MSG, (unsigned char *)_msg, strlen(_msg)+1);

    return msg_size;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <server.priv> <address> <vcs0.pub> [ ... <vcsN.pub> ]\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    ECPDHKey key_perma;
    ECPNode node;
    ecp_ecdh_public_t node_pub;
    int rv, i;

    if ((argc < 4) || (argc > 7)) usage(argv[0]);

    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);

    ecp_conn_handler_init(&handler, handle_msg, handle_open, NULL, NULL);
    ecp_ctx_set_handler(&ctx, &handler, CTYPE_TEST);

    rv = ecp_util_load_key(&key_perma.public, &key_perma.private, argv[1]);
    printf("ecp_util_load_key RV:%d\n", rv);
    key_perma.valid = 1;

    rv = ecp_sock_create(&sock, &ctx, &key_perma);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock, NULL);
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_vlink_create(&conn, &sock);
    printf("ecp_vlink_create RV:%d\n", rv);

    if (argc > 4) {
        ecp_ecdh_public_t vconn_key[3];

        for (i=3; i<argc-1; i++) {
            rv = ecp_util_load_pub(&vconn_key[i-3], argv[i]);
            printf("ecp_util_load_pub RV:%d\n", rv);
        }

        rv = ecp_vconn_create(vconn, vconn_key, argc-4, &conn);
        printf("ecp_vconn_create RV:%d\n", rv);
    }

    rv = ecp_util_load_pub(&node_pub, argv[argc-1]);
    printf("ecp_util_load_pub RV:%d\n", rv);

    rv = ecp_node_init(&node, &node_pub, argv[2]);
    printf("ecp_node_init RV:%d\n", rv);

    rv = ecp_vconn_open(&conn, &node);
    printf("ecp_vconn_open RV:%d\n", rv);

    while (1) sleep(1);
}
