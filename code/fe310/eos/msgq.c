#include <stddef.h>

#include "eos.h"
#include "msgq.h"

#define EOS_MSGQ_IDX_MASK(IDX, SIZE)  ((IDX) & ((SIZE) - 1))

void eos_msgq_init(EOSMsgQ *msgq, EOSMsgItem *array, uint8_t size) {
    msgq->idx_r = 0;
    msgq->idx_w = 0;
    msgq->array = array;
    msgq->size = size;
}

int eos_msgq_push(EOSMsgQ *msgq, unsigned char cmd, unsigned char *buffer, uint16_t len) {
    if (msgq->idx_w - msgq->idx_r == msgq->size) return EOS_ERR_Q_FULL;

    uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_w, msgq->size);
    msgq->array[idx].cmd = cmd;
    msgq->array[idx].buffer = buffer;
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

void eos_msgq_get(EOSMsgQ *msgq, unsigned char cmd, unsigned char **buffer, uint16_t *len) {
    uint8_t idx = EOS_MSGQ_IDX_MASK(msgq->idx_r, msgq->size);
    unsigned char _cmd = msgq->array[idx].cmd;
    EOSMsgItem *tmp_item = &msgq->array[idx];
    
    *buffer = msgq->array[idx].buffer;
    *len = msgq->array[idx].len;
    if (_cmd == cmd) {
        msgq->idx_r++;
        return;
    }
    for (idx = msgq->idx_r + 1; idx < msgq->idx_w; idx++) {
        *tmp_item = msgq->array[EOS_MSGQ_IDX_MASK(idx, msgq->size)];
        msgq->array[EOS_MSGQ_IDX_MASK(idx, msgq->size)].cmd = _cmd;
        msgq->array[EOS_MSGQ_IDX_MASK(idx, msgq->size)].buffer = *buffer;
        msgq->array[EOS_MSGQ_IDX_MASK(idx, msgq->size)].len = *len;
        _cmd = tmp_item->cmd;
        *buffer = tmp_item->buffer;
        *len = tmp_item->len;
        if (_cmd == cmd) {
            msgq->idx_r++;
            return;
        }
    }
    *buffer = NULL;
    *len = 0;
}

