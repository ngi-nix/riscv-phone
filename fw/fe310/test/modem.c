#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <event.h>
#include <spi.h>
#include <uart.h>
#include <net.h>
#include <cell.h>

#include <unicode.h>

#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_text.h>

#include <eve/screen/screen.h>
#include <eve/screen/window.h>
#include <eve/screen/view.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/root.h>

#include "modem.h"

typedef struct {
    uint32_t mem;
    EVEViewStack *stack;
    eos_evt_handler_t cell_dev_handler;
    EVEText text;
} VParam;

static void key_down(void *p, int c) {
    EVEView *view = p;
    EVEText *text = &((VParam *)view->param)->text;
    unsigned char *buf;
    int i = 2;

    if (c == 0x11) {
        app_modem_close(view);
        return;
    }
    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_DEV | EOS_CELL_MTYPE_UART_DATA;
    if (c == '\n') {
        buf[1] = '\r';
        buf[2] = '\n';
        i++;
    } else {
        buf[1] = c;
    }

    eos_net_send(EOS_NET_MTYPE_CELL, buf, i, 0);
    eve_text_scroll0(text);
}

static void handle_uart(unsigned char type) {
    int i = 0;
    int c = 0;
    unsigned char *buf = NULL;

    c = eos_uart_getc(0);
    if (c == EOS_ERR_EMPTY) return;

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_DEV | EOS_CELL_MTYPE_UART_DATA;
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

static void handle_cell_msg(unsigned char type, unsigned char *buffer, uint16_t len) {
    EVEScreen *screen = app_screen();
    EVEWindow *window = eve_window_get(screen, "main");
    VParam *param = window->view->param;

    if (type == EOS_CELL_MTYPE_UART_DATA) {
        EVEText *text = &param->text;
        int i;

        eos_spi_dev_start(EOS_DEV_DISP);
        for (i=1; i<len; i++) {
            if (buffer[i] != '\r') eve_text_putc(text, buffer[i]);
        }
        if (text->dirty) {
            text->dirty = 0;
            eve_screen_draw(window->screen);
        }
        eos_spi_dev_stop();
        eos_net_free(buffer, 0);
    } else {
        param->cell_dev_handler(type, buffer, len);
    }
}

static int modem_touch(EVEView *view, uint8_t tag0, int touch_idx) {
    VParam *param = view->param;
    EVEText *text = &param->text;

    return eve_text_touch(text, tag0, touch_idx);
}

static uint8_t modem_draw(EVEView *view, uint8_t tag) {
    VParam *param = view->param;
    EVEText *text = &param->text;

    return eve_text_draw(text, tag);
}

void app_modem(EVEWindow *window, EVEViewStack *stack) {
    unsigned char *buf;
    EVEScreen *screen = window->screen;
    EVEKbd *kbd = eve_screen_get_kbd(screen);
    EVERect g = {0, 60, 480, 512};
    EVEView *view;
    VParam *param;

    view = eve_malloc(sizeof(EVEView));
    param = eve_malloc(sizeof(VParam));
    param->mem = screen->mem_next;
    param->stack = stack;
    param->cell_dev_handler = eos_cell_get_handler(EOS_CELL_MTYPE_DEV);
    eve_text_init(&param->text, &g, 30, 16, 200, screen->mem_next, &screen->mem_next);
    eve_view_init(view, window, modem_touch, modem_draw, param);

    eve_kbd_set_handler(kbd, key_down, view);
    eve_screen_show_kbd(screen);

    eos_uart_set_handler(EOS_UART_ETYPE_RX, handle_uart);
    eos_cell_set_handler(EOS_CELL_MTYPE_DEV, handle_cell_msg);
    eos_net_acquire_for_evt(EOS_EVT_UART | EOS_UART_ETYPE_RX, 1);

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_DEV | EOS_CELL_MTYPE_UART_TAKE;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, 1, 0);
    eos_uart_rxwm_set(0);
}

void app_modem_close(EVEView *view) {
    unsigned char *buf = eos_net_alloc();
    VParam *param = view->param;
    EVEWindow *window = view->window;
    EVEScreen *screen = window->screen;
    EVEViewStack *stack = param->stack;

    buf[0] = EOS_CELL_MTYPE_DEV | EOS_CELL_MTYPE_RESET;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, 1, 0);
    eos_uart_rxwm_clear();
    eos_uart_set_handler(EOS_UART_ETYPE_RX, NULL);
    eos_cell_set_handler(EOS_CELL_MTYPE_DEV, param->cell_dev_handler);
    eos_net_acquire_for_evt(EOS_EVT_UART | EOS_UART_ETYPE_RX, 0);

    screen->mem_next = param->mem;
    eve_free(param);
    eve_free(view);

    eve_screen_hide_kbd(screen);
    eve_view_destroy(window, stack);
}
