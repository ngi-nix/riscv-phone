#include <stdlib.h>
#include <stdint.h>

#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "cell.h"

static uint8_t cell_mode;

static void cell_handler(unsigned char _mtype, unsigned char *buffer, uint16_t size) {
    uint8_t mtype;

    if (size < 1) return;
    mtype = buffer[0];
    switch (mtype & EOS_CELL_MTYPE_MASK) {
        case EOS_CELL_MTYPE_DEV:
            switch (mtype) {
                case EOS_CELL_MTYPE_UART_DATA:
                    if (eos_modem_get_mode() == EOS_CELL_UART_MODE_RELAY) eos_modem_write(buffer+1, size-1);
                    break;

                case EOS_CELL_MTYPE_UART_TAKE:
                    cell_mode = eos_modem_get_mode();
                    eos_modem_set_mode(EOS_CELL_UART_MODE_RELAY);
                    break;

                case EOS_CELL_MTYPE_UART_GIVE:
                    eos_modem_set_mode(cell_mode);
                    break;

                case EOS_CELL_MTYPE_PCM_DATA:
                    eos_cell_pcm_push(buffer+1, size-1);
                    break;

                case EOS_CELL_MTYPE_PCM_START:
                    eos_cell_pcm_start();
                    break;

                case EOS_CELL_MTYPE_PCM_STOP:
                    eos_cell_pcm_stop();
                    break;
            }
            break;

        case EOS_CELL_MTYPE_VOICE:
            eos_cell_voice_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;

        case EOS_CELL_MTYPE_SMS:
            eos_cell_sms_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;

        case EOS_CELL_MTYPE_USSD:
            eos_cell_ussd_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;

        case EOS_CELL_MTYPE_DATA:
            eos_cell_data_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;
    }
}

void eos_cell_init(void) {
    eos_net_set_handler(EOS_NET_MTYPE_CELL, cell_handler);
}

