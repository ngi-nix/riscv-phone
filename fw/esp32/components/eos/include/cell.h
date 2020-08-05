#include <sys/types.h>
#include <stdint.h>

#define EOS_CELL_MTYPE_DEV          0x00
#define EOS_CELL_MTYPE_VOICE        0x10
#define EOS_CELL_MTYPE_SMS          0x20
#define EOS_CELL_MTYPE_CBS          0x30
#define EOS_CELL_MTYPE_USSD         0x40
#define EOS_CELL_MTYPE_DATA         0x70

#define EOS_CELL_MTYPE_MASK         0xf0
#define EOS_CELL_MAX_MTYPE          8

#define EOS_CELL_MTYPE_READY        0
#define EOS_CELL_MTYPE_UART_DATA    1
#define EOS_CELL_MTYPE_UART_TAKE    2
#define EOS_CELL_MTYPE_UART_GIVE    3
#define EOS_CELL_MTYPE_PCM_DATA     4
#define EOS_CELL_MTYPE_PCM_START    5
#define EOS_CELL_MTYPE_PCM_STOP     6

#define EOS_CELL_MTYPE_VOICE_DIAL   1
#define EOS_CELL_MTYPE_VOICE_RING   2
#define EOS_CELL_MTYPE_VOICE_ANSWER 3
#define EOS_CELL_MTYPE_VOICE_HANGUP 4
#define EOS_CELL_MTYPE_VOICE_BEGIN  5
#define EOS_CELL_MTYPE_VOICE_END    6

#define EOS_CELL_MTYPE_USSD_REQUEST 1
#define EOS_CELL_MTYPE_USSD_REPLY   2

#define EOS_CELL_UART_MODE_NONE     0
#define EOS_CELL_UART_MODE_ATCMD    1
#define EOS_CELL_UART_MODE_PPP      2
#define EOS_CELL_UART_MODE_RELAY    3

#define EOS_CELL_UART_SIZE_BUF      128

void eos_cell_init(void);

void eos_modem_init(void);
void eos_modem_flush(void);
size_t eos_modem_write(void *data, size_t size);
size_t eos_modem_read(void *data, size_t size, uint32_t timeout);
int eos_modem_readln(char *buf, size_t buf_size, uint32_t timeout);
int eos_modem_resp(char *ok_str, char *err_str, uint32_t timeout);
uint8_t eos_modem_get_mode(void);
int eos_modem_set_mode(uint8_t mode);
int eos_modem_take(uint32_t timeout);
void eos_modem_give(void);
void eos_modem_sleep(uint8_t mode);
void eos_modem_wake(uint8_t source, uint8_t mode);

void eos_ppp_set_apn(char *apn);
void eos_ppp_set_auth(char *user, char *pass);
int eos_ppp_connect(void);
int eos_ppp_disconnect(void);

void eos_cell_pcm_init(void);
ssize_t eos_cell_pcm_read(unsigned char *data, size_t size);
int eos_cell_pcm_push(unsigned char *data, size_t size);
void eos_cell_pcm_start(void);
void eos_cell_pcm_stop(void);

void eos_cell_voice_handler(unsigned char mtype, unsigned char *buffer, uint16_t size);
void eos_cell_sms_handler(unsigned char mtype, unsigned char *buffer, uint16_t size);
void eos_cell_ussd_handler(unsigned char mtype, unsigned char *buffer, uint16_t size);
void eos_cell_data_handler(unsigned char mtype, unsigned char *buffer, uint16_t size);
