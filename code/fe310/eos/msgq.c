#include <stddef.h>
#include <string.h>

#include "eos.h"
#include "msgq.h"

#define EOS_MSGQ_IDX_MASK(IDX, SIZE)    ((IDX) & ((SIZE) - 1))

#define EOS_MSGQ_IDX_HALF               ((uint8_t)1 << (sizeof(uint8_t) * 8 - 1))
#define EOS_MSGQ_IDX_LT(a,b)            ((uint8_t)((uint8_t)(a) - (uint8_t)(b)) > EOS_MSGQ_IDX_HALF)
#define EOS_MSGQ_IDX_LTE(a,b)           ((uint8_t)((uint8_t)(b) - (uint8_t)(a)) < EOS_MSGQ_IDX_HALF)


void eos_msgq_init(EOSMsgQ *msgq, EOSMsgItem *array, uint8_t size) {
    msgq->idx_r = 0;
    msgq->idx_w = 0;
    msgq->array = array;
    msgq->size = size;
}

int eos_msgq_push(EOSMsgQ *msgq, unsigned char type, unsigned char *buffer, uint16_t len) {
    if ((uint8_t)(msgq->idx_w - msgq->idx_r) == msgq->size) return EOS_ERR_Q_FULL;

    uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_w, msgq->size);
    msgq->array[idx].type = type;
    msgq->array[idx].buffer = buffer;
    msgq->array[idx].len = len;
    msgq->idx_w++;
    return EOS_OK;
}

void eos_msgq_pop(EOSMsgQ *msgq, unsigned char *type, unsigned char **buffer, uint16_t *len) {
    if (msgq->idx_r == msgq->idx_w) {
        *type = 0;
        *buffer = NULL;
    } else {
        uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_r, msgq->size);
        *type = msgq->array[idx].type;
        *buffer = msgq->array[idx].buffer;
        *len = msgq->array[idx].len;
        msgq->idx_r++;
    }
}

int eos_msgq_get(EOSMsgQ *msgq, unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len) {
    uint8_t i, j, idx;
    
    if (msgq->idx_r == msgq->idx_w) {
        *buffer = NULL;
        *len = 0;
        return 0;
    }

    idx = EOS_MSGQ_IDX_MASK(msgq->idx_r, msgq->size);
    if (type == msgq->array[idx].type) {
        *buffer = msgq->array[idx].buffer;
        *len = msgq->array[idx].len;
        if ((selector == NULL) || (sel_len == 0) || ((sel_len <= *len) && (memcmp(selector, *buffer, sel_len) == 0))) {
            msgq->idx_r++;
            return 1;
        }
    }
    for (i = msgq->idx_r + 1; EOS_MSGQ_IDX_LT(i, msgq->idx_w); i++) {
        idx = EOS_MSGQ_IDX_MASK(i, msgq->size);
        if (type== msgq->array[idx].type) {
            *buffer = msgq->array[idx].buffer;
            *len = msgq->array[idx].len;
            if ((selector == NULL) || (sel_len == 0) || ((sel_len <= *len) && (memcmp(selector, *buffer, sel_len) == 0))) {
                for (j = i + 1; EOS_MSGQ_IDX_LT(j, msgq->idx_w); j++) {
                    msgq->array[EOS_MSGQ_IDX_MASK(j - 1, msgq->size)] = msgq->array[EOS_MSGQ_IDX_MASK(j, msgq->size)];
                }
                msgq->idx_w--;
                return 1;
            }
        }
    }
    *buffer = NULL;
    *len = 0;
    return 0;
}

