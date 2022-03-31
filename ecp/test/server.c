#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <core.h>

#include <util.h>

ECPContext ctx;
ECPSocket sock;
ECPDHKey key_perma;
ECPConnHandler handler;

#define CTYPE_TEST  0
#define MTYPE_MSG   0

static ssize_t handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    char *_msg = "VAISTINU JE CAR!";
    ssize_t rv;

    printf("MSG S:%s size:%ld\n", msg, msg_size);
    rv = ecp_msg_send(conn, MTYPE_MSG, (unsigned char *)_msg, strlen(_msg)+1);

    return msg_size;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <address> <node.priv>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    ECPDHKey key_perma;
    int rv;

    /* server */
    if (argc != 3) usage(argv[0]);

    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);

    ecp_conn_handler_init(&handler, handle_msg, NULL, NULL, NULL);
    ecp_ctx_set_handler(&ctx, &handler, CTYPE_TEST);

    rv = ecp_util_load_key(&key_perma.public, &key_perma.private, argv[2]);
    printf("ecp_util_load_key RV:%d\n", rv);
    key_perma.valid = 1;

    rv = ecp_sock_create(&sock, &ctx, &key_perma);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock, argv[1]);
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    while (1) sleep(1);
}
