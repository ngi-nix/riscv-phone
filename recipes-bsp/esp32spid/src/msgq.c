#include <stdlib.h>
#include <pthread.h>

#include "msgq.h"

#define IDX_MASK(IDX, SIZE)     ((IDX) & ((SIZE) - 1))

int msgq_init(MSGQueue *msgq, unsigned char **array, uint16_t size) {
    int rv;

    msgq->idx_r = 0;
    msgq->idx_w = 0;
    msgq->size = size;
    msgq->array = array;
    rv = pthread_mutex_init(&msgq->mutex, NULL);
    if (rv) {
        return MSGQ_ERR;
    }

    rv = pthread_cond_init(&msgq->cond, NULL);
    if (rv) {
        pthread_mutex_destroy(&msgq->mutex);
        return MSGQ_ERR;
    }
}

int msgq_push(MSGQueue *msgq, unsigned char *buffer) {
    if ((uint16_t)(msgq->idx_w - msgq->idx_r) == msgq->size) return MSGQ_ERR_FULL;

    msgq->array[IDX_MASK(msgq->idx_w++, msgq->size)] = buffer;
    return MSGQ_OK;
}

unsigned char *msgq_pop(MSGQueue *msgq) {
    if (msgq->idx_r == msgq->idx_w) return NULL;

    return msgq->array[IDX_MASK(msgq->idx_r++, msgq->size)];
}

uint16_t msgq_len(MSGQueue *msgq) {
    return (uint16_t)(msgq->idx_w - msgq->idx_r);
}
