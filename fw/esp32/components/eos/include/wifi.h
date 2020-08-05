#define EOS_WIFI_MTYPE_SCAN         0
#define EOS_WIFI_MTYPE_CONNECT      1
#define EOS_WIFI_MTYPE_DISCONNECT   2

#define EOS_WIFI_MAX_MTYPE          3

void eos_wifi_init(void);

int eos_wifi_scan(void);
int eos_wifi_set_auth(char *ssid, char *pass);
int eos_wifi_connect(void);
int eos_wifi_disconnect(void);
