#include <sys/types.h>

#define EOS_CELL_MTYPE_DATA         0
#define EOS_CELL_MTYPE_AUDIO        1

#define EOS_CELL_MTYPE_DATA_START   2
#define EOS_CELL_MTYPE_DATA_STOP    3

#define EOS_CELL_MTYPE_AUDIO_START  4
#define EOS_CELL_MTYPE_AUDIO_STOP   5

#define EOS_CELL_UART_MODE_NONE     0
#define EOS_CELL_UART_MODE_PPP      1
#define EOS_CELL_UART_MODE_RELAY    2

void eos_pcm_init(void);

ssize_t eos_pcm_read(unsigned char *data, size_t size);
int eos_pcm_push(unsigned char *data, size_t size);
void eos_pcm_start(void);
void eos_pcm_stop(void);

void eos_modem_init(void);
ssize_t eos_modem_write(void *data, size_t size);
void eos_modem_set_mode(char mode);

void eos_cell_init(void);