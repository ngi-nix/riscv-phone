#include <stdint.h>

#define EOS_OK                  0
#define EOS_ERR                 -1
#define EOS_ERR_TIMEOUT         -2
#define EOS_ERR_BUSY            -3

#define EOS_ERR_SIZE            -10
#define EOS_ERR_FULL            -11
#define EOS_ERR_EMPTY           -12
#define EOS_ERR_NOTFOUND        -13

#define EOS_ERR_NET             -20

void eos_init(void);
void eos_run(uint8_t wakeup_cause);