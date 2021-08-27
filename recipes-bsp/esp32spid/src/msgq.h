#include <stdint.h>

#define MSGQ_OK		0
#define MSGQ_ERR	-1
#define MSGQ_ERR_FULL	-10

typedef struct MSGQueue {
    uint16_t idx_r;
    uint16_t idx_w;
    uint16_t size;
    unsigned char **array;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} MSGQueue;

int msgq_init(MSGQueue *msgq, unsigned char **array, uint16_t size);
int msgq_push(MSGQueue *msgq, unsigned char *buffer);
unsigned char *msgq_pop(MSGQueue *msgq);
uint16_t msgq_len(MSGQueue *msgq);
