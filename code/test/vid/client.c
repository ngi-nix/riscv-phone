#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "core.h"
#include "util.h"

#include "vpx/vpx_decoder.h"
#include "vpx/vp8cx.h"
#include "tools.h"

#define CTYPE_TEST      0
#define MTYPE_MSG       8

#define FRAG_BUF_SIZE   8192
#define RBUF_MSG_SIZE   128

ECPContext ctx_c;
ECPSocket sock_c;
ECPConnHandler handler_c;

ECPNode node;
ECPConnection conn;

FILE *outfile;
vpx_codec_ctx_t codec;

ECPRBRecv rbuf_recv;
ECPRBMessage rbuf_r_msg[RBUF_MSG_SIZE];
ECPFragIter frag_iter;
unsigned char frag_buffer[FRAG_BUF_SIZE];

ssize_t handle_msg(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *f, ssize_t sz) {
    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = NULL;
    if (vpx_codec_decode(&codec, f, (unsigned int)sz, NULL, 0))
      die_codec(&codec, "Failed to decode frame.");

    while ((img = vpx_codec_get_frame(&codec, &iter)) != NULL) {
      if (!vpx_img_write_f(img, outfile)) {
          die_codec(NULL, "Failed to write image.");
      }
    }
    
    return sz;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <node.pub>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if (argc != 2) usage(argv[0]);
    
    rv = ecp_init(&ctx_c);
    printf("ecp_init RV:%d\n", rv);

    if (!rv) rv = ecp_conn_handler_init(&handler_c);
    if (!rv) {
        handler_c.msg[MTYPE_MSG] = handle_msg;
        ctx_c.handler[CTYPE_TEST] = &handler_c;
    }
    
    if (!rv) rv = ecp_sock_create(&sock_c, &ctx_c, NULL);
    printf("ecp_sock_create RV:%d\n", rv);

    if (!rv) rv = ecp_sock_open(&sock_c, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    if (!rv) rv = ecp_start_receiver(&sock_c);
    printf("ecp_start_receiver RV:%d\n", rv);

    if (!rv) rv = ecp_util_node_load(&ctx_c, &node, argv[1]);
    printf("ecp_util_node_load RV:%d\n", rv);

    if (!rv) rv = ecp_conn_create(&conn, &sock_c, CTYPE_TEST);
    printf("ecp_conn_create RV:%d\n", rv);

    if (!rv) rv = ecp_rbuf_create(&conn, NULL, NULL, 0, &rbuf_recv, rbuf_r_msg, RBUF_MSG_SIZE);
    printf("ecp_rbuf_create RV:%d\n", rv);
    
    if (!rv) {
        ecp_frag_iter_init(&frag_iter, frag_buffer, FRAG_BUF_SIZE);
        rbuf_recv.frag_iter = &frag_iter;
    }

    if (!rv) rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);
    
    const char *codec_arg = "vp9";
    const VpxInterface *decoder = get_vpx_decoder_by_name(codec_arg);
    if (!decoder) die_codec(NULL, "Unknown input codec.");

    printf("Using %s\n", vpx_codec_iface_name(decoder->codec_interface()));

    if (vpx_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
      die_codec(&codec, "Failed to initialize decoder.");
    
    while (1) sleep(1);

    if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
}