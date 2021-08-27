#include <stdint.h>

#define TUN_OK      0
#define TUN_ERR     -1

void *tun_read(void *arg);
int tun_write(unsigned char *buffer, uint16_t len);
int tun_init(void);