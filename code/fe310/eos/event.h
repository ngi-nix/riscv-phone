#include <stdint.h>

#define EOS_EVT_MASK_NET    0x80
#define EOS_EVT_TIMER       0x01
#define EOS_EVT_UI          0x02

#define EOS_EVT_SIZE_Q      4

typedef void (*eos_evt_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_evtq_init(void);
int eos_evtq_push(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_pop(unsigned char *cmd, unsigned char **buffer, uint16_t *len);
void eos_evtq_handle(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_loop(void);
void eos_evtq_set_handler(unsigned char cmd, eos_evt_fptr_t handler);