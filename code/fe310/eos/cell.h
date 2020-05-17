#include <stdint.h>
#include "event.h"

#define EOS_CELL_MTYPE_READY        0
#define EOS_CELL_MTYPE_DATA         1
#define EOS_CELL_MTYPE_AUDIO        2

#define EOS_CELL_MTYPE_DATA_START   4
#define EOS_CELL_MTYPE_DATA_STOP    5

#define EOS_CELL_MTYPE_AUDIO_START  6
#define EOS_CELL_MTYPE_AUDIO_STOP   7

#define EOS_CELL_MAX_MTYPE          4

void eos_cell_init(void);
void eos_cell_set_handler(unsigned char mtype, eos_evt_handler_t handler);