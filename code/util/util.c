#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "core.h"
#include "cr.h"
#include "util.h"

int ecp_util_key_save(ECPContext *ctx, ECPDHKey *key, char *filename) {
    int fd;
    ssize_t rv;
    
    if ((fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) return ECP_ERR;
    rv = write(fd, ecp_cr_dh_pub_get_buf(&key->public), ECP_ECDH_SIZE_KEY);
    if (rv != ECP_ECDH_SIZE_KEY) {
        close(fd);
        return ECP_ERR;
    }
    rv = write(fd, &key->private, sizeof(key->private));
    if (rv != sizeof(key->private)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);
    return ECP_OK;
}

int ecp_util_key_load(ECPContext *ctx, ECPDHKey *key, char *filename) {
    int fd;
    ssize_t rv;
    unsigned char buf[ECP_ECDH_SIZE_KEY];
    
    if ((fd = open(filename, O_RDONLY)) < 0) return ECP_ERR;
    rv = read(fd, buf, ECP_ECDH_SIZE_KEY);
    if (rv != ECP_ECDH_SIZE_KEY) {
        close(fd);
        return ECP_ERR;
    }
    rv = read(fd, &key->private, sizeof(key->private));
    if (rv != sizeof(key->private)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);

    ecp_cr_dh_pub_from_buf(&key->public, buf);
    
    key->valid = 1;
    return ECP_OK;
}

int ecp_util_node_save(ECPContext *ctx, ECPNode *node, char *filename) {
    int fd;
    ssize_t rv;
    
    if ((fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) return ECP_ERR;
    rv = write(fd, ecp_cr_dh_pub_get_buf(&node->public), ECP_ECDH_SIZE_KEY);
    if (rv != ECP_ECDH_SIZE_KEY) {
        close(fd);
        return ECP_ERR;
    }
    rv = write(fd, &node->addr, sizeof(node->addr));
    if (rv != sizeof(node->addr)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);
    return ECP_OK;
}

int ecp_util_node_load(ECPContext *ctx, ECPNode *node, char *filename) {
    int fd;
    ssize_t rv;
    unsigned char buf[ECP_ECDH_SIZE_KEY];
    
    if ((fd = open(filename, O_RDONLY)) < 0) return ECP_ERR;
    rv = read(fd, buf, ECP_ECDH_SIZE_KEY);
    if (rv != ECP_ECDH_SIZE_KEY) {
        close(fd);
        return ECP_ERR;
    }
    rv = read(fd, &node->addr, sizeof(node->addr));
    if (rv != sizeof(node->addr)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);

    ecp_cr_dh_pub_from_buf(&node->public, buf);

    return ECP_OK;
}