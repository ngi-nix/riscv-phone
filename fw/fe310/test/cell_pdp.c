#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <net.h>
#include <cell.h>

#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_font.h>

#include <eve/screen/window.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/app_root.h>

#include "status.h"
#include "cell_pdp.h"

static void cell_pdp_connect(char *apn, char *user, char *pass) {
    unsigned char *buffer, *p;

    buffer = eos_net_alloc();
    buffer[0] = EOS_CELL_MTYPE_PDP | EOS_CELL_MTYPE_PDP_CONFIG;
    p = buffer + 1;
    strcpy(p, user);
    p += strlen(user) + 1;
    strcpy(p, user);
    p += strlen(user) + 1;
    strcpy(p, pass);
    p += strlen(pass) + 1;
    eos_net_send(EOS_NET_MTYPE_CELL, buffer, p - buffer, 1);

    buffer = eos_net_alloc();
    buffer[0] = EOS_CELL_MTYPE_PDP | EOS_CELL_MTYPE_PDP_CONNECT;
    eos_net_send(EOS_NET_MTYPE_CELL, buffer, 1, 0);
}

static void cell_pdp_disconnect(void) {
    unsigned char *buffer = eos_net_alloc();
    buffer[0] = EOS_CELL_MTYPE_PDP | EOS_CELL_MTYPE_PDP_DISCONNECT;
    eos_net_send(EOS_NET_MTYPE_CELL, buffer, 1, 0);
}

static void cell_pdp_handler(unsigned char type, unsigned char *buffer, uint16_t size) {
    switch (type) {
        case EOS_CELL_MTYPE_PDP_CONNECT:
            app_status_msg_set("Cell data connected", 1);
            break;

        case EOS_CELL_MTYPE_PDP_DISCONNECT:
            app_status_msg_set("Cell data disconnected", 1);
            break;

        default:
            break;
    }
    eos_net_free(buffer, 0);
}

void app_cell_pdp(EVEWindow *window, EVEViewStack *stack) {
    EVEWidgetSpec spec[] = {
        {
            .label.g.w = APP_SCREEN_W / 3,
            .label.title = "APN:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.spec.str.str_size = 128,
        },
        {
            .label.g.w = APP_SCREEN_W / 3,
            .label.title = "User:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.spec.str.str_size = 128,
        },
        {
            .label.g.w = APP_SCREEN_W / 3,
            .label.title = "Pass:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.spec.str.str_size = 128,
        },
    };

    EVEForm *form = eve_form_create(window, stack, spec, 3, NULL, app_cell_pdp_action, app_cell_pdp_close);
}

void app_cell_pdp_action(EVEForm *form) {
    EVEStrWidget *apn = (EVEStrWidget *)eve_page_widget(&form->p, 0);
    EVEStrWidget *user = (EVEStrWidget *)eve_page_widget(&form->p, 1);
    EVEStrWidget *pass = (EVEStrWidget *)eve_page_widget(&form->p, 2);

    cell_pdp_connect(apn->str, user->str, pass->str);
}

void app_cell_pdp_close(EVEForm *form) {
    eve_form_destroy(form);
}

void app_cell_pdp_init(void) {
    eos_cell_set_handler(EOS_CELL_MTYPE_PDP, cell_pdp_handler);
}
