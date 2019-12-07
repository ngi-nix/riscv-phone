#include <stdint.h>
#include "event.h"

#define EOS_WIFI_MTYPE_SCAN         0
#define EOS_WIFI_MTYPE_CONNECT      1
#define EOS_WIFI_MTYPE_DISCONNECT   2

#define EOS_WIFI_MAX_MTYPE          3

void eos_wifi_init(void);
void eos_wifi_set_handler(int mtype, eos_evt_fptr_t handler, uint8_t flags);

void eos_wifi_connect(const char *ssid, const char *pass);
void eos_wifi_disconnect(void);
