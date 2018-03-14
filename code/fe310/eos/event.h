#include <stdint.h>

#define EOS_EVT_FLAG_WRAP   0x1

#define EOS_EVT_NET         0x10
#define EOS_EVT_TIMER       0x20
#define EOS_EVT_AUDIO       0x30
#define EOS_EVT_UI          0x40

#define EOS_EVT_MASK        0xF0

#define EOS_EVT_MAX_EVT     4
#define EOS_EVT_SIZE_Q      4

typedef void (*eos_evt_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_evtq_init(void);
int eos_evtq_push(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_pop(unsigned char *cmd, unsigned char **buffer, uint16_t *len);
void eos_evtq_bad_handler(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_handler_wrapper(unsigned char cmd, unsigned char *buffer, uint16_t len, uint16_t *flags_acq, uint16_t flag, eos_evt_fptr_t f);
void eos_evtq_handle(unsigned char cmd, unsigned char *buffer, uint16_t len);
void eos_evtq_loop(void);
void eos_evtq_set_handler(unsigned char cmd, eos_evt_fptr_t handler, uint8_t flags);
