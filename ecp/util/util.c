#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <core.h>

#include "util.h"

int ecp_util_load_key(ecp_ecdh_public_t *public, ecp_ecdh_private_t *private, char *filename) {
    int fd;
    ssize_t rv;

    if ((fd = open(filename, O_RDONLY)) < 0) return ECP_ERR;
    rv = read(fd, public, sizeof(ecp_ecdh_public_t));
    if (rv != sizeof(ecp_ecdh_public_t)) {
        close(fd);
        return ECP_ERR;
    }
    rv = read(fd, private, sizeof(ecp_ecdh_private_t));
    if (rv != sizeof(ecp_ecdh_private_t)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);
    return ECP_OK;
}

int ecp_util_save_key(ecp_ecdh_public_t *public, ecp_ecdh_private_t *private, char *filename) {
    int fd;
    ssize_t rv;

    if ((fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) return ECP_ERR;
    rv = write(fd, public, sizeof(ecp_ecdh_public_t));
    if (rv != sizeof(ecp_ecdh_public_t)) {
        close(fd);
        return ECP_ERR;
    }
    rv = write(fd, private, sizeof(ecp_ecdh_private_t));
    if (rv != sizeof(ecp_ecdh_private_t)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);
    return ECP_OK;
}

int ecp_util_load_pub(ecp_ecdh_public_t *public, char *filename) {
    int fd;
    ssize_t rv;

    if ((fd = open(filename, O_RDONLY)) < 0) return ECP_ERR;
    rv = read(fd, public, sizeof(ecp_ecdh_public_t));
    if (rv != sizeof(ecp_ecdh_public_t)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);
    return ECP_OK;
}

int ecp_util_save_pub(ecp_ecdh_public_t *public, char *filename) {
    int fd;
    ssize_t rv;

    if ((fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) return ECP_ERR;
    rv = write(fd, public, sizeof(ecp_ecdh_public_t));
    if (rv != sizeof(ecp_ecdh_public_t)) {
        close(fd);
        return ECP_ERR;
    }
    close(fd);
    return ECP_OK;
}

int ecp_util_load_node(ECPNode *node, char *filename) {
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

int ecp_util_save_node(ECPNode *node, char *filename) {
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
