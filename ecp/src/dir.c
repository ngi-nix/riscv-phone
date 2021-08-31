#include "core.h"
#include "cr.h"

#include "dir.h"

void ecp_dir_parse_item(unsigned char *buf, ECPDirItem *item) {
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

void ecp_dir_serialize_item(unsigned char *buf, ECPDirItem *item) {
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
