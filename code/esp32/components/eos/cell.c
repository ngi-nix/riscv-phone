#include <stdlib.h>
#include <stdint.h>

#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "cell.h"

static void cell_handler(unsigned char _mtype, unsigned char *buffer, uint16_t size) {
    uint8_t mtype = buffer[0];

    switch (mtype) {
        case EOS_CELL_MTYPE_DATA:
            eos_modem_write(buffer+1, size-1);
            break;
        case EOS_CELL_MTYPE_DATA_START:
            eos_modem_set_mode(EOS_CELL_UART_MODE_RELAY);
            break;
        case EOS_CELL_MTYPE_DATA_STOP:
            eos_modem_set_mode(0);
            break;
        case EOS_CELL_MTYPE_AUDIO:
            eos_pcm_push(buffer+1, size-1);
            break;
        case EOS_CELL_MTYPE_AUDIO_START:
            eos_pcm_start();
            break;
        case EOS_CELL_MTYPE_AUDIO_STOP:
            eos_pcm_stop();
            break;
    }
}

void eos_cell_init(void) {
    eos_pcm_init();
    eos_modem_init();
    eos_net_set_handler(EOS_NET_MTYPE_CELL, cell_handler);
}

