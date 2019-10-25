#include <stdint.h>

#define EOS_FE310_CMD_FLAG_ONEW         0x10

#define EOS_FE310_CMD_WIFI_CONNECT      1
#define EOS_FE310_CMD_WIFI_DISCONNECT   2
#define EOS_FE310_CMD_WIFI_SCAN         3
#define EOS_FE310_CMD_WIFI_PKT          4
#define EOS_FE310_CMD_MODEM_DATA        5
#define EOS_FE310_CMD_MODEM_CALL        6

#define EOS_FE310_MAX_CMD               8
#define EOS_FE310_SIZE_Q                64
#define EOS_FE310_SIZE_BUF              2048

typedef void (*eos_fe310_fptr_t) (unsigned char, unsigned char *, uint16_t);

void eos_fe310_init(void);
int eos_fe310_send(unsigned char cmd, unsigned char *buffer, uint16_t len);
