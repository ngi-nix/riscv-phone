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

#include "app_root.h"
#include "app_status.h"

#include "../phone.h"

static char status_msg[128];

int app_status_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    unsigned char state = 0;
    int8_t touch_idx;

    // state = app_phone_state_get();
    touch_idx = eve_touch_get_idx(touch);
    if (touch_idx != 0) return 0;

    evt = eve_touch_evt(touch, evt, tag0, view->tag, 2);
    if (touch && (evt & EVE_TOUCH_ETYPE_POINT_UP)) {
        if ((state == VOICE_STATE_RING) && (touch->eevt & EVE_TOUCH_EETYPE_TRACK_LEFT)) {
            unsigned char *buf = eos_net_alloc();

            buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_ANSWER;
            eos_net_send_async(EOS_NET_MTYPE_CELL, buf, 1, 0);
            status_msg[0] = '\0';
        }
        if ((state != VOICE_STATE_IDLE) && (touch->eevt & EVE_TOUCH_EETYPE_TRACK_RIGHT)) {
            unsigned char *buf = eos_net_alloc();

            buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_HANGUP;
            eos_net_send_async(EOS_NET_MTYPE_CELL, buf, 1, 0);
            status_msg[0] = '\0';
        }
        return 1;
    }
    return 0;
}

uint8_t app_status_draw(EVEView *view, uint8_t tag0) {
    uint8_t tag_opt = EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY;

    tag0 = eve_view_clear(view, tag0, tag_opt);

    if (tag0 != EVE_NOTAG) {
        eve_touch_set_opt(tag0, eve_touch_get_opt(tag0) | tag_opt);
        eve_cmd_dl(TAG(tag0));
        tag0++;
    }

    eve_cmd(CMD_TEXT, "hhhhs", 0, 0, 31, 0, status_msg);

    return tag0;
}

void app_status_msg_set(char *msg, int refresh) {
    strcpy(status_msg, msg);

    if (refresh) app_root_refresh();
}
