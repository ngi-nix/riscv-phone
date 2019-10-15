#include <stdint.h>

typedef struct EOSMsgItem {
    unsigned char type;
    unsigned char *buffer;
    uint16_t len;
} EOSMsgItem;

typedef struct EOSMsgQ {
    uint8_t idx_r;
    uint8_t idx_w;
    uint8_t size;
    EOSMsgItem *array;
} EOSMsgQ;

void eos_msgq_init(EOSMsgQ *msgq, EOSMsgItem *array, uint8_t size);
int eos_msgq_push(EOSMsgQ *msgq, unsigned char type, unsigned char *buffer, uint16_t len);
void eos_msgq_pop(EOSMsgQ *msgq, unsigned char *type, unsigned char **buffer, uint16_t *len);
int eos_msgq_get(EOSMsgQ *msgq, unsigned char type, unsigned char *selector, uint16_t sel_len, unsigned char **buffer, uint16_t *len);