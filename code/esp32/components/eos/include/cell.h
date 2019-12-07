#include <sys/types.h>

#define EOS_CELL_MTYPE_DATA         0
#define EOS_CELL_MTYPE_AUDIO        1

#define EOS_CELL_MTYPE_DATA_START   2
#define EOS_CELL_MTYPE_DATA_STOP    3

#define EOS_CELL_MTYPE_AUDIO_START  2
#define EOS_CELL_MTYPE_AUDIO_STOP   3

void eos_pcm_init(void);
ssize_t eos_pcm_write(void *data, size_t size);
void eos_pcm_call(void);

void eos_modem_init(void);
ssize_t eos_modem_write(void *data, size_t size);
void eos_modem_set_mode(char mode);
