#include <stdlib.h>

#include "eos.h"
#include "msgq.h"

#define IDX_MASK(IDX, SIZE)     ((IDX) & ((SIZE) - 1))

void eos_msgq_init(EOSMsgQ *msgq, EOSMsgItem *array, uint8_t size) {
    msgq->idx_r = 0;
    msgq->idx_w = 0;
    msgq->size = size;
    msgq->array = array;
}

int eos_msgq_push(EOSMsgQ *msgq, unsigned char type, unsigned char *buffer, uint16_t len, uint8_t flags) {
    if ((uint8_t)(msgq->idx_w - msgq->idx_r) == msgq->size) return EOS_ERR_FULL;

    uint8_t idx = IDX_MASK(msgq->idx_w, msgq->size);
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
        *len = 0;
        if (flags) *flags = 0;
    } else {
        uint8_t idx = IDX_MASK(msgq->idx_r, msgq->size);
        *type = msgq->array[idx].type;
        *buffer = msgq->array[idx].buffer;
        *len = msgq->array[idx].len;
        if (flags) *flags = msgq->array[idx].flags;
        msgq->idx_r++;
    }
}

uint8_t eos_msgq_size(EOSMsgQ *msgq) {
    return (uint8_t)(msgq->idx_w - msgq->idx_r);
}

void eos_bufq_init(EOSBufQ *bufq, unsigned char **array, uint8_t size) {
    bufq->idx_r = 0;
    bufq->idx_w = 0;
    bufq->size = size;
    bufq->array = array;
}

int eos_bufq_push(EOSBufQ *bufq, unsigned char *buffer) {
    if ((uint8_t)(bufq->idx_w - bufq->idx_r) == bufq->size) return EOS_ERR_FULL;

    bufq->array[IDX_MASK(bufq->idx_w++, bufq->size)] = buffer;
    return EOS_OK;
}

unsigned char *eos_bufq_pop(EOSBufQ *bufq) {
    if (bufq->idx_r == bufq->idx_w) return NULL;

    return bufq->array[IDX_MASK(bufq->idx_r++, bufq->size)];
}

uint8_t eos_bufq_size(EOSBufQ *bufq) {
    return (uint8_t)(bufq->idx_w - bufq->idx_r);
}
