#include <stdint.h>

#define EOS_WIFI_MTYPE_SCAN         0
#define EOS_WIFI_MTYPE_CONNECT      1
#define EOS_WIFI_MTYPE_DISCONNECT   2

#define EOS_WIFI_MAX_MTYPE          3

typedef void (*eos_wifi_fptr_t) (unsigned char *, uint16_t);

void eos_wifi_init(void);
void eos_wifi_connect(const char *ssid, const char *pass);
void eos_wifi_disconnect(void);
void eos_wifi_set_handler(int mtype, eos_wifi_fptr_t handler, uint8_t flags);