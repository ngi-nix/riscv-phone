#include "core.h"

#include <string.h>

#define SEQ_HALF            ((ecp_seq_t)1 << (sizeof(ecp_seq_t)*8-1))
#define ACK_FULL            (~(ecp_ack_t)0)
#define ACK_SIZE            (sizeof(ecp_ack_t)*8)
#define ACK_MASK_FIRST      ((ecp_ack_t)1 << (ACK_SIZE - 1))

#define SEQ_LT(a,b)         ((ecp_seq_t)((ecp_seq_t)(a) - (ecp_seq_t)(b)) > SEQ_HALF)
#define SEQ_LTE(a,b)        ((ecp_seq_t)((ecp_seq_t)(b) - (ecp_seq_t)(a)) < SEQ_HALF)

#define IDX_MASK(a)         (a & (ECP_MAX_RBUF_MSGR-1))
/* If ECP_MAX_RBUF_MSGR not pow of 2:
#define IDX_MASK(a)         (a % ECP_MAX_RBUF_MSGR)
*/

static int msg_idx(ECPRBuffer *rbuf, ecp_seq_t seq) {
    ecp_seq_t seq_offset = seq - rbuf->seq_start;
        
    if (seq_offset < ECP_MAX_RBUF_MSGR) return IDX_MASK(rbuf->msg_start + seq_offset);
    return ECP_ERR_RBUF_IDX;
}

static int msg_store(ECPRBuffer *rbuf, ecp_seq_t seq, unsigned char *msg, size_t msg_size, unsigned char delivered) {
    int idx;
    ecp_seq_t seq_offset = seq - rbuf->seq_start;
    if (seq_offset >= ECP_MAX_RBUF_MSGR) return ECP_ERR_RBUF_IDX;
    
    if (SEQ_LT(rbuf->seq_max, seq)) rbuf->seq_max = seq;
    if (delivered) return ECP_OK;
    
    idx = msg_idx(rbuf, seq);
    if (idx < 0) return idx;
    if (rbuf->msg[idx].present) return ECP_ERR_RBUF_DUP;
    
    rbuf->msg[idx].present = 1;

    return ECP_OK;
}

static int msg_flush(ECPConnection *conn, ECPRBuffer *rbuf) {
    int idx = rbuf->msg_start;
    ecp_seq_t msg_cnt = rbuf->seq_max - rbuf->seq_start + 1;
    ecp_seq_t i = 0;
    
    for (i=0; i<msg_cnt; i++) {
        if (rbuf->reliable && !rbuf->msg[idx].present) break;
        if (rbuf->deliver_delay && msg_cnt - i < rbuf->deliver_delay) break;
        if (rbuf->msg[idx].present) {
            rbuf->msg[idx].present = 0;
            // deliver idx
        }
        idx = IDX_MASK(idx + 1);
    }
    rbuf->msg_start = idx;
    rbuf->seq_start += i;
    
    return ECP_OK;
}

static int ack_shift(ECPRBuffer *rbuf) {
    int do_ack = 0;
    int idx;
    int i;
    
    if (rbuf->reliable && ((rbuf->ack_map & ACK_MASK_FIRST) == 0)) return 0;

    idx = msg_idx(rbuf, rbuf->seq_ack);
    if (idx < 0) return idx;

    while (SEQ_LT(rbuf->seq_ack, rbuf->seq_max)) {
        idx = IDX_MASK(idx + 1);
        rbuf->seq_ack++;

        if (rbuf->msg[idx].present && (rbuf->ack_map == ACK_FULL)) continue;
        
        rbuf->ack_map = rbuf->ack_map << 1;
        rbuf->ack_map |= rbuf->msg[idx].present;
        if (!do_ack && !rbuf->msg[idx].present && SEQ_LTE(rbuf->seq_ack, rbuf->seq_max - 2 * rbuf->hole_max)) {
            do_ack = 1;
        }

        if ((rbuf->ack_map & ACK_MASK_FIRST) == 0) break;
    }
    
    if (!do_ack && (rbuf->seq_ack == rbuf->seq_max) && ((rbuf->ack_map & rbuf->hole_mask_full) != rbuf->hole_mask_full)) {
        ecp_ack_t hole_mask = rbuf->ack_map;

        for (i=0; i<rbuf->hole_max-1; i++) {
            hole_mask = hole_mask >> 1;
            if ((hole_mask & rbuf->hole_mask_empty) == 0) {
                do_ack = 1;
                break;
            }
        }
    }
    
    return do_ack;
}

int ecp_rbuf_recv_init(ECPRBuffer *rbuf, ecp_seq_t seq, unsigned char hole_max) {
    memset(rbuf, 0, sizeof(ECPRBuffer));
    rbuf->hole_max = hole_max;
    rbuf->hole_mask_full = ~(~((ecp_ack_t)0) << (hole_max * 2));
    rbuf->hole_mask_empty = ~(~((ecp_ack_t)0) << (hole_max + 1));
    rbuf->seq_ack = seq;
    rbuf->seq_max = seq;
    rbuf->seq_start = seq + 1;
    rbuf->ack_map = ACK_FULL;
    
    return ECP_OK;
}

ssize_t ecp_rbuf_recv_store(ECPConnection *conn, ecp_seq_t seq, unsigned char *msg, size_t msg_size) {
    int rv;
    int do_ack = 0;
    ECPRBuffer *rbuf = conn->rbuf.recv;
    
    if (rbuf == NULL) return ECP_ERR;
    
    if (SEQ_LTE(seq, rbuf->seq_ack)) {
        if ((ecp_seq_t)(rbuf->seq_ack - seq) < ACK_SIZE) {
            rv = msg_store(rbuf, seq, msg, msg_size, 0);
            if (rv) return rv;

            rbuf->ack_map |= (1 << (ecp_seq_t)(rbuf->seq_ack - seq));
            do_ack = ack_shift(rbuf);
        } else {
            return ECP_ERR_RBUF_IDX;
        }
    } else {
        if ((rbuf->ack_map == ACK_FULL) && (seq == (ecp_seq_t)(rbuf->seq_ack + 1))) {
            rv = msg_store(rbuf, seq, msg, msg_size, !rbuf->deliver_delay);
            if (rv) return rv;
            
            if (!rbuf->deliver_delay) {
                rbuf->seq_start++;
                // deliver
            }
            rbuf->seq_ack++;
        } else {
            rv = msg_store(rbuf, seq, msg, msg_size, 0);
            if (rv) return rv;

            do_ack = ack_shift(rbuf);
        }
    }
    // XXX
    // update do_ack for pps
    // should send acks more aggresively when ack_map is not full (freq > RTT)
    // now send acks as per do_ack
    return msg_size;
}