#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <eos/eos.h>
#include <eos/event.h>
#include <eos/timer.h>
#include <eos/uart.h>
#include <eos/i2s.h>
#include <eos/spi.h>
#include <eos/net.h>
#include <eos/cell.h>
#include <eos/eve/eve.h>
#include <eos/eve/eve_kbd.h>
#include <eos/eve/eve_text.h>

#define ABUF_SIZE       512
#define MIC_WM          128

static uint8_t mic_arr[ABUF_SIZE];
static uint8_t spk_arr[ABUF_SIZE];

static int audio_on = 0;

static EVEText box;
static EVEKbd kbd;

static void key_down(void *p, int c) {
    int i = 2;
    unsigned char *buf = eos_net_alloc();

    buf[0] = EOS_CELL_MTYPE_UART_DATA;
    if (c == '\n') {
        buf[1] = '\r';
        buf[2] = '\n';
        i++;
    } else {
        buf[1] = c;
    }

    eos_net_send(EOS_NET_MTYPE_CELL, buf, i, 0);
}

static void handle_touch(void *p, uint8_t tag0, int touch_idx) {
    eve_kbd_touch(&kbd, tag0, touch_idx);
    eve_text_touch(&box, tag0, touch_idx);

	eve_cmd_dl(CMD_DLSTART);
	eve_cmd_dl(CLEAR_COLOR_RGB(0,0,0));
	eve_cmd_dl(CLEAR(1,1,1));
    eve_text_draw(&box);
    eve_kbd_draw(&kbd);
    eve_cmd_dl(DISPLAY());
	eve_cmd_dl(CMD_SWAP);
    eve_cmd_exec(1);
}

static void handle_mic(unsigned char type) {
    uint16_t size;
    unsigned char *buf = eos_net_alloc();

    buf[0] = EOS_CELL_MTYPE_PCM_DATA;
    size = eos_i2s_mic_read(buf+1, MIC_WM);
    eos_net_send(EOS_NET_MTYPE_CELL, buf, size+1, 0);
}

static void handle_uart(unsigned char type) {
    int i = 0;
    int c = 0;
    unsigned char *buf = NULL;

    c = eos_uart_getc(0);
    if (c == EOS_ERR_EMPTY) return;

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_UART_DATA;
    buf[1] = c;
    i = 2;
    while ((c = eos_uart_getc(0)) != EOS_ERR_EMPTY) {
        buf[i] = c;
        i++;
        if (i == EOS_NET_SIZE_BUF) break;
    }
    eos_net_send(EOS_NET_MTYPE_CELL, buf, i, 0);
    eos_uart_rxwm_set(0);
}

static void handle_cell_msg(unsigned char _type, unsigned char *buffer, uint16_t len) {
    int i;
    unsigned char type = buffer[0];

    switch (type) {
        case EOS_CELL_MTYPE_READY:
            buffer[0] = EOS_CELL_MTYPE_UART_TAKE;
            eos_net_send(EOS_NET_MTYPE_CELL, buffer, 1, 0);
            eos_uart_rxwm_set(0);
            printf("\nREADY.\n");
            break;

        case EOS_CELL_MTYPE_UART_DATA:
        eos_spi_dev_start(EOS_DEV_DISP);
            for (i=1; i<len; i++) {
                if (buffer[i] != '\r') eve_text_putc(&box, buffer[i]);
            }
            eos_net_free(buffer, 0);
            if (box.dirty) handle_touch(NULL, 0, -1);
            eos_spi_dev_stop();
            break;

        case EOS_CELL_MTYPE_PCM_DATA:
            eos_i2s_spk_write(buffer+1, len-1);
            eos_net_free(buffer, 0);
            break;

        default:
            eos_net_free(buffer, 0);
            break;
    }
}

static void audio_toggle(unsigned char *buf) {
    if (audio_on) {
        audio_on = 0;
        buf[0] = EOS_CELL_MTYPE_PCM_STOP;
        eos_net_send(EOS_NET_MTYPE_CELL, buf, 1, 0);
        eos_i2s_stop();
    } else {
        audio_on = 1;
        buf[0] = EOS_CELL_MTYPE_PCM_START;
        eos_net_send(EOS_NET_MTYPE_CELL, buf, 1, 0);
        eos_i2s_start(8000, EOS_I2S_FMT_PCM16);
    }
}

int main() {
    EVERect g = {0, 60, 480, 512};
    uint32_t mem_next = EVE_RAM_G;

    eos_init();

    eos_spi_dev_start(EOS_DEV_DISP);

    eve_text_init(&box, &g, 30, 16, 0x80, 200, mem_next, &mem_next);
    eve_kbd_init(&kbd, NULL, mem_next, &mem_next);
    eve_kbd_set_handler(&kbd, key_down, NULL);

    eve_touch_set_handler(handle_touch, NULL);
    eve_touch_set_opt(0x80, EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_EXT);
    handle_touch(NULL, 0, -1);
    eve_brightness(0x40);

    eos_spi_dev_stop();

    eos_i2s_mic_init(mic_arr, ABUF_SIZE);
    eos_i2s_mic_set_wm(MIC_WM);
    eos_i2s_mic_set_handler(handle_mic);
    eos_i2s_spk_init(spk_arr, ABUF_SIZE);

    eos_uart_set_handler(EOS_UART_ETYPE_RX, handle_uart);
    eos_cell_set_handler(EOS_CELL_MTYPE_DEV, handle_cell_msg);

    eos_net_acquire_for_evt(EOS_EVT_I2S | EOS_I2S_ETYPE_MIC, 1);
    eos_net_acquire_for_evt(EOS_EVT_UI | EVE_ETYPE_INTR, 1);
    eos_net_acquire_for_evt(EOS_EVT_UART | EOS_UART_ETYPE_RX, 1);

    eos_evtq_loop();
}
