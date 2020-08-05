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
uint8_t eos_msgq_len(EOSMsgQ *msgq);

typedef struct EOSBufQ {
    uint8_t idx_r;
    uint8_t idx_w;
    uint8_t size;
    unsigned char **array;
} EOSBufQ;

void eos_bufq_init(EOSBufQ *bufq, unsigned char **array, uint8_t size);
int eos_bufq_push(EOSBufQ *bufq, unsigned char *buffer);
unsigned char *eos_bufq_pop(EOSBufQ *bufq);
uint8_t eos_bufq_len(EOSBufQ *bufq);
