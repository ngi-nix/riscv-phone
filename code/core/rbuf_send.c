#include "core.h"

#include <string.h>

static ssize_t handle_ack(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    return size;
}

ssize_t ecp_rbuf_send_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size) {
    return ecp_rbuf_msg_store(&conn->rbuf.send->rbuf, seq, msg, msg_size, 0, 0);
}