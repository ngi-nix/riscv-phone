#include <stddef.h>
#include <string.h>

#include "eos.h"
#include "msgq.h"

#define EOS_MSGQ_IDX_MASK(IDX, SIZE)    ((IDX) & ((SIZE) - 1))

void eos_msgq_init(EOSMsgQ *msgq, EOSMsgItem *array, uint8_t size) {
    msgq->idx_r = 0;
    msgq->idx_w = 0;
    msgq->array = array;
    msgq->size = size;
}

int eos_msgq_push(EOSMsgQ *msgq, unsigned char cmd, unsigned char *buffer, uint16_t len) {
    if ((uint8_t)(msgq->idx_w - msgq->idx_r) == msgq->size) return EOS_ERR_Q_FULL;

    uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_w, msgq->size);
    msgq->array[idx].cmd = cmd;
    memcpy(msgq->array[idx].buffer, buffer, len);
    msgq->array[idx].len = len;
    msgq->idx_w++;
    return EOS_OK;
}

void eos_msgq_pop(EOSMsgQ *msgq, unsigned char *cmd, unsigned char **buffer, uint16_t *len) {
    if (msgq->idx_r == msgq->idx_w) {
        *cmd = 0;
        *buffer = NULL;
    } else {
        uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_r, msgq->size);
        *cmd = msgq->array[idx].cmd;
        *buffer = msgq->array[idx].buffer;
        *len = msgq->array[idx].len;
        msgq->idx_r++;
    }
}

