#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <core.h>

#include "util.h"

#define FN_LEN  256

static char fn_key[FN_LEN];
static char fn_node[FN_LEN];

static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <name> [address]\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    char *addr;
    ECPDHKey key;
    ECPNode node;
    int rv;

    if ((argc < 2) || (argc > 3)) usage(argv[0]);

    addr = NULL;
    if (argc == 3) addr = argv[2];

    if (strlen(argv[1]) > FN_LEN - 6) usage(argv[0]);
    strcpy(fn_node, argv[1]);
    strcpy(fn_key, argv[1]);
    strcat(fn_key, ".priv");
    strcat(fn_node, ".pub");

    rv = ecp_dhkey_gen(&key);
    if (rv) goto err;

    rv = ecp_node_init(&node, &key.public, addr);
    if (rv) goto err;

    rv = ecp_util_save_key(&key.public, &key.private, fn_key);
    if (rv) goto err;

    if (addr) {
        rv = ecp_util_save_node(&node, fn_node);
        if (rv) goto err;
    } else {
        rv = ecp_util_save_pub(&key.public, fn_node);
        if (rv) goto err;
    }

    return 0;

err:
    printf("ERR:%d\n", rv);
    return 1;
}