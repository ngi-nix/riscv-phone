#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "core.h"
#include "cr.h"
#include "util.h"

int ecp_util_key_save(ECPDHKey *key, char *filename) {
    int fd;
    ssize_t rv;

    if (!key->valid) return ECP_ERR;

    if ((fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) return ECP_ERR;
    rv = write(fd, &key->public, sizeof(key->public));
    if (rv != sizeof(key->public)) {
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

int ecp_util_key_load(ECPDHKey *key, char *filename) {
    int fd;
    ssize_t rv;

    if ((fd = open(filename, O_RDONLY)) < 0) return ECP_ERR;
    rv = read(fd, &key->public, sizeof(key->public));
    if (rv != sizeof(key->public)) {
        close(fd);
        return ECP_ERR;
    }
    rv = read(fd, &key->private, sizeof(key->private));
    if (rv != sizeof(key->private)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);

    key->valid = 1;
    return ECP_OK;
}

int ecp_util_node_save(ECPNode *node, char *filename) {
    int fd;
    ssize_t rv;

    if (!node->key_perma.valid) return ECP_ERR;

    if ((fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) return ECP_ERR;
    rv = write(fd, &node->key_perma.public, sizeof(node->key_perma.public));
    if (rv != sizeof(node->key_perma.public)) {
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

int ecp_util_node_load(ECPNode *node, char *filename) {
    int fd;
    ssize_t rv;

    if ((fd = open(filename, O_RDONLY)) < 0) return ECP_ERR;
    rv = read(fd, &node->key_perma.public, sizeof(node->key_perma.public));
    if (rv != sizeof(node->key_perma.public)) {
        close(fd);
        return ECP_ERR;
    }
    rv = read(fd, &node->addr, sizeof(node->addr));
    if (rv != sizeof(node->addr)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);

    node->key_perma.valid = 1;
    return ECP_OK;
}