#include "core.h"

#include <string.h>

#define ACK_RATE_UNIT   10000

static ssize_t handle_ack(ECPConnection *conn, ecp_seq_t seq, unsigned char mtype, unsigned char *msg, ssize_t size) {
    int idx = 0;
    ecp_seq_t seq_ack = 0;
    ecp_ack_t ack_map = 0;
    
    ECPRBSend *buf = conn->rbuf.send;
    ECPRBuffer *rbuf = &buf->rbuf;
    
    if (ack_map != ECP_RBUF_ACK_FULL) {
        int i;
        int nack = 0;
        ecp_win_t ack_cnt = 0;
        
        seq_ack -= ECP_RBUF_ACK_SIZE - 1;

        idx = ecp_rbuf_msg_idx(rbuf, seq_ack);
        if (idx < 0) return idx;

        if (buf->reliable) buf->in_transit -= seq_ack - rbuf->seq_start;
        for (i=0; i<ECP_RBUF_ACK_SIZE; i++) {
            if (ack_map & ((ecp_ack_t)1 << (ECP_RBUF_ACK_SIZE - i - 1))) {
                ack_cnt++;
            } else if (buf->reliable) {
                // if resend packet (seq_ack + i)
                if (!nack) {
                    nack = 1;
                    
                    rbuf->seq_start = seq_ack + i;
                    rbuf->msg_start = ECP_RBUF_IDX_MASK(idx + i, rbuf->msg_size);
                }
            }
        }
        if (buf->reliable) {
            buf->in_transit -= ack_cnt;
        } else {
            rbuf->seq_start = seq_ack + ECP_RBUF_ACK_SIZE;
            buf->nack_rate = (buf->nack_rate * 7 + ((ECP_RBUF_ACK_SIZE - ack_cnt) * ACK_RATE_UNIT) / ECP_RBUF_ACK_SIZE) / 8;
        }
    } else {
        seq_ack++;

        idx = ecp_rbuf_msg_idx(rbuf, seq_ack);
        if (idx < 0) return idx;
        
        rbuf->seq_start = seq_ack;
        if (buf->reliable) {
            buf->in_transit -= seq_ack - rbuf->seq_start;
            rbuf->msg_start = idx;
        } else {
            buf->nack_rate = (buf->nack_rate * 7) / 8;
        }
    }
    return size;
}

int ecp_rbuf_send_create(ECPRBSend *buf, ECPRBMessage *msg, unsigned int msg_size) {
    memset(buf, 0, sizeof(ECPRBRecv));
    memset(msg, 0, sizeof(ECPRBMessage) * msg_size);
    ecp_rbuf_init(&buf->rbuf, msg, msg_size);
    return ECP_OK;
}

ssize_t ecp_rbuf_send_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size) {
    ECPRBSend *buf = conn->rbuf.send;
    if (buf->reliable) buf->in_transit++;
    return ecp_rbuf_msg_store(&buf->rbuf, seq, msg, msg_size, 0, 0);
}