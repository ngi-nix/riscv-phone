#include "core.h"
#include "cr.h"

#include "dir.h"

static int dir_update(ECPDirList *list, ECPDirItem *item) {
    int i;

    for (i=0; i<list->count; i++) {
        if (memcmp(ecp_cr_dh_pub_get_buf(&list->item[i].node.public), ecp_cr_dh_pub_get_buf(&item->node.public), ECP_ECDH_SIZE_KEY) == 0) {
            return ECP_OK;
        }
    }

    if (list->count == ECP_MAX_DIR_ITEM) return ECP_ERR_SIZE;

    list->item[list->count] = *item;
    list->count++;

    return ECP_OK;
}

ssize_t ecp_dir_parse(ECPDirList *list, unsigned char *buf, size_t buf_size) {
    ECPDirItem item;
    size_t size;
    int rv;

    size = buf_size;
    while (size >= ECP_SIZE_DIR_ITEM) {
        ecp_dir_item_parse(&item, buf);

        rv = dir_update(list, &item);
        if (rv) return rv;

        buf += ECP_SIZE_DIR_ITEM;
        size -= ECP_SIZE_DIR_ITEM;
    };

    return buf_size - size;
}

int ecp_dir_serialize(ECPDirList *list, unsigned char *buf, size_t buf_size) {
    int i;

    for (i=0; i<list->count; i++) {
        if (buf_size < ECP_SIZE_DIR_ITEM) return ECP_ERR_SIZE;

        ecp_dir_item_serialize(&list->item[i], buf);
        buf += ECP_SIZE_DIR_ITEM;
        buf_size -= ECP_SIZE_DIR_ITEM;
    }

    return ECP_OK;
}

void ecp_dir_item_parse(ECPDirItem *item, unsigned char *buf) {
    ecp_cr_dh_pub_from_buf(&item->node.public, buf);
    buf += ECP_ECDH_SIZE_KEY;

    memcpy(&item->node.addr.host, buf, sizeof(item->node.addr));
    buf += ECP_IPv4_ADDR_SIZE;

    item->node.addr.port =  \
        (buf[0] << 8) |     \
        (buf[1]);
    buf += sizeof(uint16_t);

    item->capabilities =    \
        (buf[0] << 8) |     \
        (buf[1]);
    buf += sizeof(uint16_t);
}

void ecp_dir_item_serialize(ECPDirItem *item, unsigned char *buf) {
    ecp_cr_dh_pub_to_buf(buf, &item->node.public);
    buf += ECP_ECDH_SIZE_KEY;

    memcpy(buf, &item->node.addr.host, sizeof(item->node.addr));
    buf += ECP_IPv4_ADDR_SIZE;

    buf[0] = (item->node.addr.port & 0xFF00) >> 8;
    buf[1] = (item->node.addr.port & 0x00FF);
    buf += sizeof(uint16_t);

    buf[0] = (item->capabilities & 0xFF00) >> 8;
    buf[1] = (item->capabilities & 0x00FF);
    buf += sizeof(uint16_t);
}
