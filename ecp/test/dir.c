#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core.h"
#include "cr.h"
#include "dir.h"
#include "dir_srv.h"

ECPContext ctx_s;
ECPSocket sock_s;
ECPDHKey key_perma_s;

ECPContext ctx_c;
ECPSocket sock_c;
ECPDHKey key_perma_c;
ECPConnHandler handler_c;

ECPNode node;
ECPConnection conn;

static ECPDirList dir_online;
static ECPDirList dir_shadow;

#define CTYPE_TEST  0

ssize_t handle_dir(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size, ECP2Buffer *b) {
    ECPDirList dir;
    ssize_t rv;

    dir.count = 0;
    rv = ecp_dir_parse(&dir, msg, size);
    if (rv < 0) return rv;

    printf("DIR: %s %ld\n", (char *)ecp_cr_dh_pub_get_buf(&dir.item[0].node.public), size);

    return rv;
}

int main(int argc, char *argv[]) {
    int rv;

    dir_online.count = 1;
    strcpy((char *)ecp_cr_dh_pub_get_buf(&dir_online.item[0].node.public), "PERA");

    rv = ecp_init(&ctx_s);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_dir_init(&ctx_s, &dir_online, &dir_shadow);
    printf("ecp_dir_init RV:%d\n", rv);

    rv = ecp_dhkey_gen(&ctx_s, &key_perma_s);
    printf("ecp_dhkey_gen RV:%d\n", rv);

    rv = ecp_sock_init(&sock_s, &ctx_s, &key_perma_s);
    printf("ecp_sock_init RV:%d\n", rv);

    rv = ecp_sock_open(&sock_s, "0.0.0.0:3000");
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock_s);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_init(&ctx_c);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_conn_handler_init(&handler_c);
    handler_c.msg[ECP_MTYPE_DIR] = handle_dir;
    ctx_c.handler[CTYPE_TEST] = &handler_c;

    rv = ecp_dhkey_gen(&ctx_c, &key_perma_c);
    printf("ecp_dhkey_gen RV:%d\n", rv);

    rv = ecp_sock_init(&sock_c, &ctx_c, &key_perma_c);
    printf("ecp_sock_init RV:%d\n", rv);

    rv = ecp_sock_open(&sock_c, NULL);
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock_c);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_node_init(&node, &key_perma_s.public, "127.0.0.1:3000");
    printf("ecp_node_init RV:%d\n", rv);

    rv = ecp_conn_init(&conn, &sock_c, CTYPE_TEST);
    printf("ecp_conn_init RV:%d\n", rv);

    rv = ecp_conn_get_dirlist(&conn, &node);
    printf("ecp_conn_get_dirlist RV:%d\n", rv);

    while (1) sleep(1);
}