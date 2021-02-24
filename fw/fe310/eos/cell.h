#include <stdint.h>
#include "event.h"

#define EOS_CELL_MTYPE_DEV              0x10
#define EOS_CELL_MTYPE_VOICE            0x20
#define EOS_CELL_MTYPE_SMS              0x30
#define EOS_CELL_MTYPE_CBS              0x40
#define EOS_CELL_MTYPE_USSD             0x50
#define EOS_CELL_MTYPE_DATA             0x70

#define EOS_CELL_MTYPE_MASK             0xf0
#define EOS_CELL_MAX_MTYPE              8

/* EOS_CELL_MTYPE_DEV subtypes */
#define EOS_CELL_MTYPE_READY            1
#define EOS_CELL_MTYPE_UART_DATA        2
#define EOS_CELL_MTYPE_UART_TAKE        3
#define EOS_CELL_MTYPE_UART_GIVE        4
#define EOS_CELL_MTYPE_PCM_DATA         5
#define EOS_CELL_MTYPE_PCM_START        6
#define EOS_CELL_MTYPE_PCM_STOP         7
#define EOS_CELL_MTYPE_RESET            8

#define EOS_CELL_MTYPE_VOICE_DIAL       1
#define EOS_CELL_MTYPE_VOICE_RING       2
#define EOS_CELL_MTYPE_VOICE_ANSWER     3
#define EOS_CELL_MTYPE_VOICE_HANGUP     4
#define EOS_CELL_MTYPE_VOICE_BEGIN      5
#define EOS_CELL_MTYPE_VOICE_END        6
#define EOS_CELL_MTYPE_VOICE_MISS       7
#define EOS_CELL_MTYPE_VOICE_BUSY       8
#define EOS_CELL_MTYPE_VOICE_ERR        9

#define EOS_CELL_MTYPE_SMS_LIST         1
#define EOS_CELL_MTYPE_SMS_SEND         2
#define EOS_CELL_MTYPE_SMS_MSG_NEW      3
#define EOS_CELL_MTYPE_SMS_MSG_ITEM     4

#define EOS_CELL_MTYPE_USSD_REQUEST     1
#define EOS_CELL_MTYPE_USSD_REPLY       2
#define EOS_CELL_MTYPE_USSD_CANCEL      3

#define EOS_CELL_MTYPE_DATA_CONFIGURE   1
#define EOS_CELL_MTYPE_DATA_CONNECT     2
#define EOS_CELL_MTYPE_DATA_DISCONNECT  3

#define EOS_CELL_SMS_ADDRTYPE_INTL      1
#define EOS_CELL_SMS_ADDRTYPE_ALPHA     2
#define EOS_CELL_SMS_ADDRTYPE_OTHER     3

void eos_cell_init(void);
void eos_cell_set_handler(unsigned char mtype, eos_evt_handler_t handler);
eos_evt_handler_t eos_cell_get_handler(unsigned char mtype);