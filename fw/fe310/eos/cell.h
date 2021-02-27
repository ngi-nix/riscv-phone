#include <stdint.h>
#include "event.h"

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

void eos_cell_init(void);
void eos_cell_set_handler(unsigned char mtype, eos_evt_handler_t handler);
eos_evt_handler_t eos_cell_get_handler(unsigned char mtype);