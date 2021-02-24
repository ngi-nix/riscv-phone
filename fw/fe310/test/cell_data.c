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

#include <eve/screen/screen.h>
#include <eve/screen/window.h>
#include <eve/screen/view.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/root.h>

#include "status.h"
#include "cell_data.h"

extern EVEFont *_app_font_default;

static void cell_data_handler(unsigned char type, unsigned char *buffer, uint16_t size) {
    switch (type) {
        case EOS_CELL_MTYPE_DATA_CONNECT:
            app_status_msg_set("Cell data connected", 1);
            break;

        case EOS_CELL_MTYPE_DATA_DISCONNECT:
            app_status_msg_set("Cell data disconnected", 1);
            break;

        default:
            break;
    }
    eos_net_free(buffer, 0);
}

void app_cell_data(EVEWindow *window, EVEViewStack *stack) {
    APPWidgetSpec spec[] = {
        {
            .label.g.w = APP_SCREEN_W / 3,
            .label.font = _app_font_default,
            .label.title = "APN:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.g.w = APP_SCREEN_W - APP_SCREEN_W / 3,
            .widget.spec.str.font = _app_font_default,
            .widget.spec.str.str_size = 128,
        },
        {
            .label.g.w = APP_SCREEN_W / 3,
            .label.font = _app_font_default,
            .label.title = "User:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.g.w = APP_SCREEN_W - APP_SCREEN_W / 3,
            .widget.spec.str.font = _app_font_default,
            .widget.spec.str.str_size = 128,
        },
        {
            .label.g.w = APP_SCREEN_W / 3,
            .label.font = _app_font_default,
            .label.title = "Pass:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.g.w = APP_SCREEN_W - APP_SCREEN_W / 3,
            .widget.spec.str.font = _app_font_default,
            .widget.spec.str.str_size = 128,
        },
    };

    EVEForm *form = app_form_create(window, stack, spec, 3, app_cell_data_action, app_cell_data_close);
}

void app_cell_data_action(EVEForm *form) {
    unsigned char *buf = eos_net_alloc();
    unsigned char *p;
    EVEStrWidget *apn = (EVEStrWidget *)eve_form_widget(form, 0);
    EVEStrWidget *user = (EVEStrWidget *)eve_form_widget(form, 1);
    EVEStrWidget *pass = (EVEStrWidget *)eve_form_widget(form, 2);

    buf[0] = EOS_CELL_MTYPE_DATA | EOS_CELL_MTYPE_DATA_CONFIGURE;
    p = buf + 1;
    strcpy(p, apn->str);
    p += strlen(apn->str) + 1;
    strcpy(p, user->str);
    p += strlen(user->str) + 1;
    strcpy(p, pass->str);
    p += strlen(pass->str) + 1;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, p - buf, 1);

    eos_net_acquire();
    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_DATA | EOS_CELL_MTYPE_DATA_CONNECT;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, 1, 0);
}

void app_cell_data_close(EVEForm *form) {
    app_form_destroy(form);
}

void app_cell_data_init(void) {
    eos_cell_set_handler(EOS_CELL_MTYPE_DATA, cell_data_handler);
}
