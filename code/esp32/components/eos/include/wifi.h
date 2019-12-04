#define EOS_WIFI_MTYPE_SCAN         0
#define EOS_WIFI_MTYPE_CONNECT      1
#define EOS_WIFI_MTYPE_DISCONNECT   2

#define EOS_WIFI_MAX_MTYPE          3

void eos_wifi_init(void);
void eos_wifi_connect(char *ssid, char *pass);
void eos_wifi_disconnect(void);
