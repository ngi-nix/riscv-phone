#include <stdlib.h>

#include "eos.h"
#include "msgq.h"

#define EOS_MSGQ_IDX_MASK(IDX, SIZE)    ((IDX) & ((SIZE) - 1))

void eos_msgq_init(EOSMsgQ *msgq, EOSMsgItem *array, uint8_t size) {
    msgq->idx_r = 0;
    msgq->idx_w = 0;
    msgq->array = array;
    msgq->size = size;
}

int eos_msgq_push(EOSMsgQ *msgq, unsigned char type, unsigned char *buffer, uint16_t len, uint8_t flags) {
    if ((uint8_t)(msgq->idx_w - msgq->idx_r) == msgq->size) return EOS_ERR_Q_FULL;

    uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_w, msgq->size);
    msgq->array[idx].type = type;
    msgq->array[idx].buffer = buffer;
    msgq->array[idx].len = len;
    msgq->array[idx].flags = flags;
    msgq->idx_w++;
    return EOS_OK;
}

void eos_msgq_pop(EOSMsgQ *msgq, unsigned char *type, unsigned char **buffer, uint16_t *len, uint8_t *flags) {
    if (msgq->idx_r == msgq->idx_w) {
        *type = 0;
        *buffer = NULL;
    } else {
        uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_r, msgq->size);
        *type = msgq->array[idx].type;
        *buffer = msgq->array[idx].buffer;
        *len = msgq->array[idx].len;
        *flags = msgq->array[idx].flags;
        msgq->idx_r++;
    }
}

