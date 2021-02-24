#include <stdint.h>
#include "event.h"

#define EOS_WIFI_MTYPE_SCAN         1
#define EOS_WIFI_MTYPE_CONNECT      2
#define EOS_WIFI_MTYPE_DISCONNECT   3

#define EOS_WIFI_MAX_MTYPE          4

void eos_wifi_init(void);
void eos_wifi_set_handler(unsigned char mtype, eos_evt_handler_t handler);
eos_evt_handler_t eos_wifi_get_handler(unsigned char mtype);

void eos_wifi_scan(void);
void eos_wifi_connect(const char *ssid, const char *pass);
void eos_wifi_disconnect(void);
