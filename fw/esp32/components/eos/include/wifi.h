#define EOS_WIFI_MTYPE_SCAN         1
#define EOS_WIFI_MTYPE_CONFIG       2
#define EOS_WIFI_MTYPE_CONNECT      3
#define EOS_WIFI_MTYPE_DISCONNECT   4

#define EOS_WIFI_MAX_MTYPE          5

void eos_wifi_init(void);

int eos_wifi_scan(void);
int eos_wifi_set_config(char *ssid, char *pass);
int eos_wifi_connect(void);
int eos_wifi_disconnect(void);
