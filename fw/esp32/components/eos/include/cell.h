#include <sys/types.h>
#include <stdint.h>

#define EOS_CELL_MTYPE_DEV              0x10
#define EOS_CELL_MTYPE_VOICE            0x20
#define EOS_CELL_MTYPE_SMS              0x30
#define EOS_CELL_MTYPE_CBS              0x40
#define EOS_CELL_MTYPE_USSD             0x50
#define EOS_CELL_MTYPE_PDP              0x60

#define EOS_CELL_MTYPE_MASK             0xf0
#define EOS_CELL_MAX_MTYPE              8

/* EOS_CELL_MTYPE_DEV subtypes */
#define EOS_CELL_MTYPE_READY            1
#define EOS_CELL_MTYPE_UART_DATA        2
#define EOS_CELL_MTYPE_UART_TAKE        3
#define EOS_CELL_MTYPE_UART_GIVE        4
#define EOS_CELL_MTYPE_RESET            5

#define EOS_CELL_MTYPE_VOICE_PCM        1
#define EOS_CELL_MTYPE_VOICE_DIAL       2
#define EOS_CELL_MTYPE_VOICE_RING       3
#define EOS_CELL_MTYPE_VOICE_ANSWER     4
#define EOS_CELL_MTYPE_VOICE_HANGUP     5
#define EOS_CELL_MTYPE_VOICE_BEGIN      6
#define EOS_CELL_MTYPE_VOICE_END        7
#define EOS_CELL_MTYPE_VOICE_MISS       8
#define EOS_CELL_MTYPE_VOICE_BUSY       9
#define EOS_CELL_MTYPE_VOICE_ERR        10

#define EOS_CELL_MTYPE_SMS_LIST         1
#define EOS_CELL_MTYPE_SMS_SEND         2
#define EOS_CELL_MTYPE_SMS_MSG_NEW      3
#define EOS_CELL_MTYPE_SMS_MSG_ITEM     4

#define EOS_CELL_MTYPE_USSD_REQUEST     1
#define EOS_CELL_MTYPE_USSD_REPLY       2
#define EOS_CELL_MTYPE_USSD_CANCEL      3

#define EOS_CELL_MTYPE_PDP_CONFIG       1
#define EOS_CELL_MTYPE_PDP_CONNECT      2
#define EOS_CELL_MTYPE_PDP_DISCONNECT   3

#define EOS_CELL_SMS_ADDRTYPE_INTL      1
#define EOS_CELL_SMS_ADDRTYPE_ALPHA     2
#define EOS_CELL_SMS_ADDRTYPE_OTHER     3

#define EOS_CELL_UART_MODE_NONE         0
#define EOS_CELL_UART_MODE_ATCMD        1
#define EOS_CELL_UART_MODE_PPP          2
#define EOS_CELL_UART_MODE_RELAY        3
#define EOS_CELL_UART_MODE_UNDEF        0xff

#define EOS_CELL_UART_SIZE_BUF          1024

#define EOS_CELL_MAX_USSD_STR           256
#define EOS_CELL_MAX_DIAL_STR           256

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
int eos_modem_reset(void);

void eos_ppp_set_apn(char *apn);
void eos_ppp_set_auth(char *user, char *pass);
int eos_ppp_connect(void);
void eos_ppp_disconnect(void);

void eos_cell_pcm_init(void);
ssize_t eos_cell_pcm_read(unsigned char *data, size_t size);
int eos_cell_pcm_push(unsigned char *data, size_t size);
void eos_cell_pcm_start(void);
void eos_cell_pcm_stop(void);

void eos_cell_voice_handler(unsigned char mtype, unsigned char *buffer, uint16_t buf_len);
void eos_cell_sms_handler(unsigned char mtype, unsigned char *buffer, uint16_t buf_len);
void eos_cell_ussd_handler(unsigned char mtype, unsigned char *buffer, uint16_t buf_len);
void eos_cell_pdp_handler(unsigned char mtype, unsigned char *buffer, uint16_t buf_len);

void eos_cell_voice_init(void);
void eos_cell_sms_init(void);
void eos_cell_ussd_init(void);
