#include <stdlib.h>
#include <string.h>

#include <core.h>
#include <cr.h>

#include "dir.h"
#include "dir_srv.h"

static int dir_update(ECPDirList *list, ECPDirItem *item) {
    int i;

    for (i=0; i<list->count; i++) {
        if (memcmp(&list->item[i].node.key_perma.public, &item->node.key_perma.public, sizeof(item->node.key_perma.public)) == 0) {
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
    size_t rsize;
    uint16_t count;
    int i;
    int rv;

    if (buf_size < sizeof(uint16_t)) return ECP_ERR_SIZE;

    count = \
        (buf[0] << 8) | \
        (buf[1]);

    rsize = sizeof(uint16_t) + count * ECP_SIZE_DIR_ITEM;
    if (buf_size < rsize) return ECP_ERR_SIZE;

    buf += sizeof(uint16_t);
    for (i=0; i<count; i++) {
        ecp_dir_item_parse(&item, buf);

        rv = dir_update(list, &item);
        if (rv) return rv;

        buf += ECP_SIZE_DIR_ITEM;
    }

    return rsize;
}

ssize_t ecp_dir_serialize(ECPDirList *list, unsigned char *buf, size_t buf_size) {
    size_t rsize;
    int i;

    rsize = sizeof(uint16_t) + list->count * ECP_SIZE_DIR_ITEM;
    if (buf_size < rsize) return ECP_ERR_SIZE;

    buf[0] = (list->count & 0xFF00) >> 8;
    buf[1] = (list->count & 0x00FF);
    buf += sizeof(uint16_t);
    for (i=0; i<list->count; i++) {
        ecp_dir_item_serialize(&list->item[i], buf);
        buf += ECP_SIZE_DIR_ITEM;
    }

    return rsize;
}

void ecp_dir_item_parse(ECPDirItem *item, unsigned char *buf) {
    ECPDHPub *key;
    ecp_tr_addr_t *addr;

    key = &item->node.key_perma;
    addr = &item->node.addr;

    memcpy(&key->public, buf, sizeof(key->public));
    buf += sizeof(key->public);

    memcpy(&addr->host, buf, sizeof(addr->host));
    buf += sizeof(addr->host);

    addr->port =            \
        (buf[0] << 8) |     \
        (buf[1]);
    buf += sizeof(uint16_t);

    item->capabilities =    \
        (buf[0] << 8) |     \
        (buf[1]);
    buf += sizeof(uint16_t);
}

void ecp_dir_item_serialize(ECPDirItem *item, unsigned char *buf) {
    ECPDHPub *key;
    ecp_tr_addr_t *addr;

    key = &item->node.key_perma;
    addr = &item->node.addr;

    memcpy(buf, &key->public, sizeof(key->public));
    buf += sizeof(key->public);

    memcpy(buf, &addr->host, sizeof(addr->host));
    buf += sizeof(addr->host);

    buf[0] = (addr->port & 0xFF00) >> 8;
    buf[1] = (addr->port & 0x00FF);
    buf += sizeof(uint16_t);

    buf[0] = (item->capabilities & 0xFF00) >> 8;
    buf[1] = (item->capabilities & 0x00FF);
    buf += sizeof(uint16_t);
}

ssize_t ecp_dir_handle(ECPConnection *conn, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    ecp_dir_handler_t handler;

    handler = ecp_get_dir_handler(conn);
    if (handler) {
        return handler(conn, msg, msg_size, b);
    } else {
        return ECP_ERR_HANDLER;
    }
}

int ecp_dir_handle_open(ECPConnection *conn, ECP2Buffer *b) {
    ssize_t rv;

    if (ecp_conn_is_inb(conn)) return ECP_OK;

#ifdef ECP_WITH_DIRSRV
    rv = ecp_dir_send_upd(conn);
#else
    rv = ecp_dir_send_req(conn);
#endif
    if (rv < 0) return rv;

    return ECP_OK;
}

ssize_t ecp_dir_handle_msg(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, size_t msg_size, ECP2Buffer *b) {
    switch (mtype) {
#ifdef ECP_WITH_DIRSRV
        case ECP_MTYPE_DIR_UPD: {
            return ecp_dir_handle_upd(conn, msg, msg_size);
        }

        case ECP_MTYPE_DIR_REQ: {
            return ecp_dir_handle_req(conn, msg, msg_size);
        }
#endif

        case ECP_MTYPE_DIR_REP: {
#ifdef ECP_WITH_DIRSRV
            return ecp_dir_handle_rep(conn, msg, msg_size);
#else
            return ecp_dir_handle(conn, msg, msg_size, b);
#endif
        }

        default:
            return ECP_ERR_MTYPE;
    }
}

static ssize_t _dir_send_req(ECPConnection *conn, ECPTimerItem *ti) {
    ECPBuffer packet;
    ECPBuffer payload;
    unsigned char pkt_buf[ECP_SIZE_PKT_BUF(0, ECP_MTYPE_DIR_REQ, conn)];
    unsigned char pld_buf[ECP_SIZE_PLD_BUF(0, ECP_MTYPE_DIR_REQ, conn)];

    packet.buffer = pkt_buf;
    packet.size = sizeof(pkt_buf);
    payload.buffer = pld_buf;
    payload.size = sizeof(pld_buf);

    ecp_pld_set_type(payload.buffer, payload.size, ECP_MTYPE_DIR_REQ);
    return ecp_pld_send_wtimer(conn, &packet, &payload, ECP_SIZE_PLD(0, ECP_MTYPE_DIR_REQ), 0, ti);
}

ssize_t ecp_dir_send_req(ECPConnection *conn) {
    return ecp_timer_send(conn, _dir_send_req, ECP_MTYPE_DIR_REP, ECP_SEND_TRIES, ECP_SEND_TIMEOUT);
}

int ecp_dir_get(ECPConnection *conn, ECPSocket *sock, ECPNode *node) {
    int rv;
    ssize_t _rv;

    rv = ecp_conn_create(conn, sock, ECP_CTYPE_DIR);
    if (rv) return rv;

    rv = ecp_conn_open(conn, node);
    if (rv) return rv;

    return ECP_OK;
}
