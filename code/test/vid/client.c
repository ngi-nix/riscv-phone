#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "vpx/vpx_decoder.h"
#include "vpx/vp8cx.h"
#include "tools.h"
#include "display.h"

#include "core.h"
#include "util.h"

#define CTYPE_TEST      0
#define MTYPE_MSG       8

#define FRAG_BUF_SIZE   8192
#define RBUF_MSG_SIZE   128

ECPContext ctx_c;
ECPSocket sock_c;
ECPConnHandler handler_c;

ECPNode node;
ECPConnection conn;

ECPVConnection vconn;
ECPNode vconn_node;

vpx_codec_ctx_t codec;

ECPRBRecv rbuf_recv;
ECPRBMessage rbuf_r_msg[RBUF_MSG_SIZE];
ECPFragIter frag_iter;
unsigned char frag_buffer[FRAG_BUF_SIZE];

SDLCanvas sdl_canvas;

ssize_t handle_msg(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *f, ssize_t sz, ECP2Buffer *b) {
    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = NULL;

    if (vpx_codec_decode(&codec, f, (unsigned int)sz, NULL, 0)) {
        fprintf(stderr, "\n%lu\n", sz);
        fprintf(stderr, "ERROR!\n");
        // die_codec(&codec, "Failed to decode frame.");
    }

    while ((img = vpx_codec_get_frame(&codec, &iter)) != NULL) {
        if (!vpx_img_write(img, sdl_canvas.yuvBuffer, sdl_canvas.yPlaneSz + 2 * sdl_canvas.uvPlaneSz)) die_codec(NULL, "Failed to write image.");
    }
    sdl_display_frame(&sdl_canvas);
    
    return sz;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <node.pub> <vcs.pub>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if (argc != 3) usage(argv[0]);
    
    rv = ecp_init(&ctx_c);
    fprintf(stderr, "ecp_init RV:%d\n", rv);

    if (!rv) rv = ecp_conn_handler_init(&handler_c);
    if (!rv) {
        handler_c.msg[MTYPE_MSG] = handle_msg;
        ctx_c.handler[CTYPE_TEST] = &handler_c;
    }
    
    if (!rv) rv = ecp_sock_create(&sock_c, &ctx_c, NULL);
    fprintf(stderr, "ecp_sock_create RV:%d\n", rv);

    if (!rv) rv = ecp_sock_open(&sock_c, NULL);
    fprintf(stderr, "ecp_sock_open RV:%d\n", rv);
    
    if (!rv) rv = ecp_start_receiver(&sock_c);
    fprintf(stderr, "ecp_start_receiver RV:%d\n", rv);

    if (!rv) rv = ecp_util_node_load(&ctx_c, &node, argv[1]);
    fprintf(stderr, "ecp_util_node_load RV:%d\n", rv);

    if (!rv) rv = ecp_util_node_load(&ctx, &vconn_node, argv[2]);
    printf("ecp_util_node_load RV:%d\n", rv);

    if (!rv) rv = ecp_conn_create(&conn, &sock_c, CTYPE_TEST);
    fprintf(stderr, "ecp_conn_create RV:%d\n", rv);

    if (!rv) rv = ecp_rbuf_create(&conn, NULL, NULL, 0, &rbuf_recv, rbuf_r_msg, RBUF_MSG_SIZE);
    fprintf(stderr, "ecp_rbuf_create RV:%d\n", rv);
    
    if (!rv) {
        ecp_frag_iter_init(&frag_iter, frag_buffer, FRAG_BUF_SIZE);
        rbuf_recv.frag_iter = &frag_iter;
    }

    const char *codec_arg = "vp9";
    const VpxInterface *decoder = get_vpx_decoder_by_name(codec_arg);
    if (!decoder) die_codec(NULL, "Unknown input codec.");

    fprintf(stderr, "Using %s\n", vpx_codec_iface_name(decoder->codec_interface()));

    if (vpx_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
      die_codec(&codec, "Failed to initialize decoder.");
    
    sdl_open(&sdl_canvas, 640, 480);
    
    // if (!rv) rv = ecp_conn_open(&conn, &node);
    // fprintf(stderr, "ecp_conn_open RV:%d\n", rv);

    if (!rv) rv = ecp_vconn_open(&conn, &node, &vconn, &vconn_node, 1);
    printf("ecp_vconn_open RV:%d\n", rv);
    
    sdl_loop();
    sdl_close(&sdl_canvas);
    if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
}