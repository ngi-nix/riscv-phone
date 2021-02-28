#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <event.h>
#include <spi.h>
#include <i2s.h>
#include <net.h>
#include <cell.h>

#include <unicode.h>

#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_font.h>

#include <eve/screen/screen.h>
#include <eve/screen/window.h>
#include <eve/screen/view.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/app_screen.h>
#include <app/app_form.h>

#include "phone.h"
#include "status.h"

static char status_msg[128];

static int status_touch(EVEView *v, uint8_t tag0, int touch_idx) {
    if (touch_idx == 0) {
        EVETouch *t;
        uint16_t evt;
        unsigned char state = app_phone_state_get();

        t = eve_touch_evt(tag0, touch_idx, v->window->tag, 2, &evt);
        if (t && (evt & EVE_TOUCH_ETYPE_POINT_UP)) {
            if ((state == VOICE_STATE_RING) && (t->eevt & EVE_TOUCH_EETYPE_TRACK_LEFT)) {
                unsigned char *buf = eos_net_alloc();

                buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_ANSWER;
                eos_net_send(EOS_NET_MTYPE_CELL, buf, 1, 0);
                status_msg[0] = '\0';
            }
            if ((state != VOICE_STATE_IDLE) && (t->eevt & EVE_TOUCH_EETYPE_TRACK_RIGHT)) {
                unsigned char *buf = eos_net_alloc();

                buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_HANGUP;
                eos_net_send(EOS_NET_MTYPE_CELL, buf, 1, 0);
                status_msg[0] = '\0';
            }
            return 1;
        }
    }
    return 0;
}

static uint8_t status_draw(EVEView *v, uint8_t tag0) {
    uint8_t tag_opt = EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY;
    if (v->window->tag != EVE_TAG_NOTAG) eve_touch_set_opt(v->window->tag, eve_touch_get_opt(v->window->tag) | tag_opt);

    if (tag0 != EVE_TAG_NOTAG) {
        eve_touch_set_opt(tag0, eve_touch_get_opt(tag0) | tag_opt);
        eve_cmd_dl(TAG(tag0));
        tag0++;
    }

    eve_cmd(CMD_TEXT, "hhhhs", 0, 0, 31, 0, status_msg);

    return tag0;
}

void app_status_msg_set(char *msg, int refresh) {
    strcpy(status_msg, msg);

    if (refresh) app_screen_refresh();
}

void app_status_init(void) {
    EVEScreen *screen = app_screen();
    EVEWindow *status = eve_window_get(screen, "status");
    status->view->touch = status_touch;
    status->view->draw = status_draw;
}
