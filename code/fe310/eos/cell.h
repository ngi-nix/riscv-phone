#include <stdint.h>
#include "event.h"

#define EOS_CELL_MTYPE_DATA         0
#define EOS_CELL_MTYPE_AUDIO        1

#define EOS_CELL_MTYPE_DATA_START   2
#define EOS_CELL_MTYPE_DATA_STOP    3

#define EOS_CELL_MTYPE_AUDIO_START  4
#define EOS_CELL_MTYPE_AUDIO_STOP   5

#define EOS_CELL_MAX_MTYPE          2

void eos_cell_init(void);
void eos_cell_set_handler(unsigned char mtype, eos_evt_handler_t handler);