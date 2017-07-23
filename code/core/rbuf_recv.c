#include "core.h"

#include <string.h>

#define ACK_RATE            8
#define ACK_MASK_FIRST      ((ecp_ack_t)1 << (ECP_RBUF_ACK_SIZE - 1))

static ssize_t msg_store(ECPRBRecv *buf, ecp_seq_t seq, unsigned char *msg, size_t msg_size) {
    ssize_t rv = ecp_rbuf_msg_store(&buf->rbuf, seq, msg, msg_size, ECP_RBUF_FLAG_PRESENT, ECP_RBUF_FLAG_PRESENT);
    if (rv < 0) return ECP_ERR_RBUF_DUP;
    
    if (ECP_RBUF_SEQ_LT(buf->seq_max, seq)) buf->seq_max = seq;
    return rv;
}

static void msg_flush(ECPConnection *conn) {
    ECPRBRecv *buf = conn->rbuf.recv;
    ecp_seq_t msg_cnt = buf->seq_max - buf->rbuf.seq_start + 1;
    ecp_seq_t i = 0;
    unsigned int idx = buf->rbuf.msg_start;
    
    for (i=0; i<msg_cnt; i++) {
        if (buf->reliable && !(buf->rbuf.msg[idx].flags & ECP_RBUF_FLAG_PRESENT)) break;
        if (buf->deliver_delay && msg_cnt - i < buf->deliver_delay) break;
        if (buf->rbuf.msg[idx].flags & ECP_RBUF_FLAG_PRESENT) {
            buf->rbuf.msg[idx].flags &= ~ECP_RBUF_FLAG_PRESENT;
            ecp_msg_handle(conn, buf->rbuf.seq_start + i, buf->rbuf.msg[idx].msg, buf->rbuf.msg[idx].size);
        }
        idx = ECP_RBUF_IDX_MASK(idx + 1, buf->rbuf.msg_size);
    }
    buf->rbuf.msg_start = idx;
    buf->rbuf.seq_start += i;
}

static int ack_shift(ECPRBRecv *buf) {
    int do_ack = 0;
    int idx;
    int i;
    
    if (buf->reliable && ((buf->ack_map & ACK_MASK_FIRST) == 0)) return 0;

    idx = ecp_rbuf_msg_idx(&buf->rbuf, buf->seq_ack);
    if (idx < 0) return idx;

    while (ECP_RBUF_SEQ_LT(buf->seq_ack, buf->seq_max)) {
        idx = ECP_RBUF_IDX_MASK(idx + 1, buf->rbuf.msg_size);
        buf->seq_ack++;

        if ((buf->rbuf.msg[idx].flags & ECP_RBUF_FLAG_PRESENT) && (buf->ack_map == ECP_RBUF_ACK_FULL)) continue;
        
        buf->ack_map = buf->ack_map << 1;
        if (buf->rbuf.msg[idx].flags & ECP_RBUF_FLAG_PRESENT) {
            buf->ack_map |= 1;
        } else if (!do_ack && ECP_RBUF_SEQ_LTE(buf->seq_ack, buf->seq_max - 2 * buf->hole_max)) {
            do_ack = 1;
        }

        if ((buf->ack_map & ACK_MASK_FIRST) == 0) break;
    }
    
    if (!do_ack && (buf->seq_ack == buf->seq_max) && ((buf->ack_map & buf->hole_mask_full) != buf->hole_mask_full)) {
        ecp_ack_t hole_mask = buf->ack_map;

        for (i=0; i<buf->hole_max-1; i++) {
            hole_mask = hole_mask >> 1;
            if ((hole_mask & buf->hole_mask_empty) == 0) {
                do_ack = 1;
                break;
            }
        }
    }
    
    return do_ack;
}

int ecp_rbuf_recv_create(ECPRBRecv *buf, ECPRBMessage *msg, unsigned int msg_size) {
    memset(buf, 0, sizeof(ECPRBRecv));
    memset(msg, 0, sizeof(ECPRBMessage) * msg_size);
    ecp_rbuf_init(&buf->rbuf, msg, msg_size);
    buf->ack_map = ECP_RBUF_ACK_FULL;
    buf->ack_rate = ACK_RATE;

    return ECP_OK;
}

int ecp_rbuf_recv_start(ECPRBRecv *buf, ecp_seq_t seq) {
    buf->seq_ack = seq;
    buf->seq_max = seq;
    buf->rbuf.seq_start = seq + 1;

    return ECP_OK;
}

int ecp_rbuf_recv_set_hole(ECPRBRecv *buf, unsigned short hole_max) {
    buf->hole_max = hole_max;
    buf->hole_mask_full = ~(~((ecp_ack_t)0) << (hole_max * 2));
    buf->hole_mask_empty = ~(~((ecp_ack_t)0) << (hole_max + 1));
    
    return ECP_OK;
}

int ecp_rbuf_recv_set_delay(ECPRBRecv *buf, unsigned short delay) {
    buf->deliver_delay = delay;
    if (buf->hole_max < delay - 1) {
        ecp_rbuf_recv_set_hole(buf, delay - 1);
    }
    
    return ECP_OK;
}

ssize_t ecp_rbuf_recv_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size) {
    ssize_t rv;
    ecp_seq_t ack_pkt = 0;
    int do_ack = 0; 
    ECPRBRecv *buf = conn->rbuf.recv;
    
    if (buf == NULL) return ECP_ERR;
    
    if (ECP_RBUF_SEQ_LT(buf->seq_max, seq)) ack_pkt = seq - buf->seq_max;
    if (ECP_RBUF_SEQ_LTE(seq, buf->seq_ack)) {
        ecp_seq_t seq_offset = buf->seq_ack - seq;
        if (seq_offset < ECP_RBUF_ACK_SIZE) {
            ecp_ack_t ack_mask = ((ecp_ack_t)1 << seq_offset);

            if (ack_mask & buf->ack_map) return ECP_ERR_RBUF_DUP;
            
            rv = msg_store(buf, seq, msg, msg_size);
            if (rv < 0) return rv;

            buf->ack_map |= ack_mask;
            do_ack = ack_shift(buf);
        } else {
            return ECP_ERR_RBUF_IDX;
        }
    } else {
        if ((buf->ack_map == ECP_RBUF_ACK_FULL) && (seq == (ecp_seq_t)(buf->seq_ack + 1))) {
            if (buf->deliver_delay) {
                rv = msg_store(buf, seq, msg, msg_size);
                if (rv < 0) return rv;
            } else {
                rv = ecp_msg_handle(conn, seq, msg, msg_size);
                buf->seq_max++;
                buf->rbuf.seq_start++;
            }
            buf->seq_ack++;
        } else {
            rv = msg_store(buf, seq, msg, msg_size);
            if (rv < 0) return rv;

            do_ack = ack_shift(buf);
        }
    }
    if (ack_pkt && !do_ack) {
        buf->ack_pkt += ack_pkt;
        // should send acks more aggresively when reliable and ack_map is not full (rate ~ PPS * RTT)
        if (buf->ack_pkt > buf->ack_rate) do_ack = 1;
    }
    if (do_ack) {
        buf->ack_pkt = 0;
        // send ack (with seq = 0)
    }
    // XXX should handle close
    msg_flush(conn);
    return rv;
}
