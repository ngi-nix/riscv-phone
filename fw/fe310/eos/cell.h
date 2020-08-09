#include <stdint.h>
#include "event.h"

#define EOS_CELL_MTYPE_DEV              0x00
#define EOS_CELL_MTYPE_VOICE            0x10
#define EOS_CELL_MTYPE_SMS              0x20
#define EOS_CELL_MTYPE_CBS              0x30
#define EOS_CELL_MTYPE_USSD             0x40
#define EOS_CELL_MTYPE_DATA             0x70

#define EOS_CELL_MTYPE_MASK             0xf0
#define EOS_CELL_MAX_MTYPE              8

#define EOS_CELL_MTYPE_READY            0
#define EOS_CELL_MTYPE_UART_DATA        1
#define EOS_CELL_MTYPE_UART_TAKE        2
#define EOS_CELL_MTYPE_UART_GIVE        3
#define EOS_CELL_MTYPE_PCM_DATA         4
#define EOS_CELL_MTYPE_PCM_START        5
#define EOS_CELL_MTYPE_PCM_STOP         6

#define EOS_CELL_MTYPE_VOICE_DIAL       1
#define EOS_CELL_MTYPE_VOICE_RING       2
#define EOS_CELL_MTYPE_VOICE_ANSWER     3
#define EOS_CELL_MTYPE_VOICE_HANGUP     4
#define EOS_CELL_MTYPE_VOICE_BEGIN      5
#define EOS_CELL_MTYPE_VOICE_END        6
#define EOS_CELL_MTYPE_VOICE_MISSED     7

#define EOS_CELL_MTYPE_SMS_LIST         1
#define EOS_CELL_MTYPE_SMS_SEND         2
#define EOS_CELL_MTYPE_SMS_MSG_NEW      3
#define EOS_CELL_MTYPE_SMS_MSG_ITEM     4

#define EOS_CELL_MTYPE_USSD_REQUEST     1
#define EOS_CELL_MTYPE_USSD_REPLY       2

#define EOS_CELL_MTYPE_DATA_CONFIGURE   1
#define EOS_CELL_MTYPE_DATA_CONNECT     2
#define EOS_CELL_MTYPE_DATA_DISCONNECT  3

#define EOS_CELL_SMS_ADDRTYPE_INTL      1
#define EOS_CELL_SMS_ADDRTYPE_ALPHA     2
#define EOS_CELL_SMS_ADDRTYPE_OTHER     3

void eos_cell_init(void);
void eos_cell_set_handler(unsigned char mtype, eos_evt_handler_t handler);