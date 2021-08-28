#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "core.h"
#include "util.h"

#define FN_LEN  256

static char fn_key[FN_LEN];
static char fn_node[FN_LEN];

static int v_rng(void *buf, size_t bufsize) {
    int fd;

    if((fd = open("/dev/urandom", O_RDONLY)) < 0) return -1;
    size_t nb = read(fd, buf, bufsize);
    close(fd);
    if (nb != bufsize) return -1;
    return 0;
}

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <name> [address]\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    ECPContext ctx;
    ECPDHKey key;
    ECPNode node;

    if ((argc < 2) || (argc > 3)) usage(argv[0]);

    if (strlen(argv[1]) > FN_LEN - 6) usage(argv[0]);
    strcpy(fn_node, argv[1]);
    strcpy(fn_key, argv[1]);
    strcat(fn_key, ".priv");
    strcat(fn_node, ".pub");

    rv = ecp_ctx_init(&ctx);
    if (rv) goto err;
    ctx.rng = v_rng;

    rv = ecp_dhkey_generate(&ctx, &key);
    if (rv) goto err;

    rv = ecp_node_init(&node, &key.public, (argc == 3) ? argv[2] : NULL);
    if (rv) goto err;

    rv = ecp_util_key_save(&ctx, &key, fn_key);
    if (rv) goto err;

    rv = ecp_util_node_save(&ctx, &node, fn_node);
    if (rv) goto err;

    return 0;

err:
    printf("ERR:%d\n", rv);
    return 1;
}