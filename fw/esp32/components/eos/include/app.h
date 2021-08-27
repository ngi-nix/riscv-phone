#include <stdint.h>

/* common */
#define EOS_APP_MTU                 1500
#define EOS_APP_SIZE_BUF            EOS_APP_MTU

#define EOS_APP_MTYPE_TUN           1
#define EOS_APP_MAX_MTYPE           8

#define EOS_APP_MTYPE_FLAG_MASK     0xc0

/* esp32 specific */
#define EOS_APP_SIZE_BUFQ           4
#define EOS_APP_SIZE_SNDQ           4

typedef void (*eos_app_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_app_init(void);

unsigned char *eos_app_alloc(void);
void eos_app_free(unsigned char *buf);
int eos_app_send(unsigned char mtype, unsigned char *buffer, uint16_t len);
void eos_app_set_handler(unsigned char mtype, eos_app_fptr_t handler);
