#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#include "core.h"
#include "vconn/vconn.h"
#include "util.h"

ECPContext ctx;
ECPSocket sock;
ECPConnHandler handler;

ECPNode node;
ECPConnection conn;

ECPVConnOut vconn[20];
ECPNode vconn_node[20];

#define CTYPE_TEST  0
#define MTYPE_MSG   8


int counter = 0;
uint64_t t_start = 0;
uint64_t t_end = 0;
ssize_t handle_open(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    ssize_t rv = ecp_conn_handle_open(conn, sq, t, p, s, b);
    if (rv < 0) {
        printf("OPEN ERR:%ld\n", s);
        return rv;
    }

    printf("OPEN\n");
    char *msg = "PERA JE CAR!";
    unsigned char buf[1000];

    strcpy((char *)buf, msg);
    // ssize_t _rv = ecp_send(conn, MTYPE_MSG, buf, 1000);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    t_start = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;

    return rv;
}

ssize_t handle_msg(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    ecp_send(conn, t, p, s);
    write(2, p+1, s-1);
    fflush(stderr);
    // printf("MSG C:%s size:%ld\n", p, s);
    return s;

    counter++;
    char *msg = "PERA JE CAR!";
    unsigned char buf[1000];

    strcpy((char *)buf, msg);
    ssize_t _rv = ecp_send(conn, MTYPE_MSG, buf, 1000);

    if (counter % 100 == 0) {
        struct timeval tv;
        uint64_t t_time;

        gettimeofday(&tv, NULL);
        t_end = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
        t_time = t_end - t_start;
        printf("T:%f\n", (float)t_time/1000000);
        t_start = t_end;
    }
    return s;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <server.pub> <vcs1.pub> ... <vcsn.pub>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv, i;

    if ((argc < 3) || (argc > 22)) usage(argv[0]);

    rv = ecp_init(&ctx);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_conn_handler_init(&handler);
    handler.msg[ECP_MTYPE_OPEN] = handle_open;
    handler.msg[MTYPE_MSG] = handle_msg;
    ctx.handler[CTYPE_TEST] = &handler;

    rv = ecp_sock_init(&sock, &ctx, NULL);
    printf("ecp_sock_init RV:%d\n", rv);

    rv = ecp_sock_open(&sock, NULL);
    printf("ecp_sock_open RV:%d\n", rv);

    rv = ecp_start_receiver(&sock);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_util_node_load(&ctx, &node, argv[1]);
    printf("ecp_util_node_load RV:%d\n", rv);

    for (i=0; i<argc-2; i++) {
        rv = ecp_util_node_load(&ctx, &vconn_node[i], argv[i+2]);
        printf("ecp_util_node_load RV:%d\n", rv);
    }

    rv = ecp_conn_init(&conn, &sock, CTYPE_TEST);
    printf("ecp_conn_init RV:%d\n", rv);

    rv = ecp_vconn_open(&conn, &node, vconn, vconn_node, argc-2);
    printf("ecp_vconn_open RV:%d\n", rv);

    while (1) sleep(1);
}
