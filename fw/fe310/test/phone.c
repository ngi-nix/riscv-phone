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

#include <eve/screen/window.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/app_root.h>
#include <app/app_form.h>

#include "status.h"
#include "phone.h"

#define ABUF_SIZE       512
#define MIC_WM          128

static uint8_t mic_arr[ABUF_SIZE];
static uint8_t spk_arr[ABUF_SIZE];

#define VOICE_STATE_IDLE    0
#define VOICE_STATE_DIAL    1
#define VOICE_STATE_RING    2
#define VOICE_STATE_CIP     3

static unsigned char voice_state = 0;

static void handle_mic(unsigned char type) {
    uint16_t size;
    unsigned char *buf = eos_net_alloc();

    buf[0] = EOS_CELL_MTYPE_VOICE_PCM;
    size = eos_i2s_mic_read(buf+1, MIC_WM);
    eos_net_send(EOS_NET_MTYPE_CELL, buf, size+1, 0);
}

static void cell_voice_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    char msg[128];

    printf("VOICE: %d\n", type);
    msg[0] = 0;
    switch (type) {
        case EOS_CELL_MTYPE_VOICE_RING:
            voice_state = VOICE_STATE_RING;
            sprintf(msg, "RING:%s", buffer+1);
            break;

        case EOS_CELL_MTYPE_VOICE_MISS:
            voice_state = VOICE_STATE_IDLE;
            break;

        case EOS_CELL_MTYPE_VOICE_BEGIN:
            voice_state = VOICE_STATE_CIP;
            eos_i2s_start(8000, EOS_I2S_FMT_PCM16);
            break;

        case EOS_CELL_MTYPE_VOICE_END:
            voice_state = VOICE_STATE_IDLE;
            eos_i2s_stop();
            break;

        case EOS_CELL_MTYPE_VOICE_PCM:
            if (voice_state == VOICE_STATE_CIP) {
                eos_i2s_spk_write(buffer+1, len-1);
            }
            break;
    }
    app_status_msg_set(msg, 1);
    eos_net_free(buffer, 0);
}

void app_phone(EVEWindow *window, EVEViewStack *stack) {
    EVEWidgetSpec spec[] = {
        {
            .label.title = "Phone:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.spec.str.str_size = 128,
        },
    };

    EVEForm *form = app_form_create(window, stack, spec, 1, app_phone_action, NULL);
}

void app_phone_action(EVEForm *form) {
    char msg[128];
    EVEStrWidget *w = (EVEStrWidget *)eve_form_widget(form, 0);
    unsigned char *buf = eos_net_alloc();

    buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_DIAL;
    strcpy(buf + 1, w->str);
    eos_net_send(EOS_NET_MTYPE_CELL, buf, 1 + strlen(w->str), 0);

    voice_state = VOICE_STATE_DIAL;
    sprintf(msg, "DIAL:%s", w->str);
    app_status_msg_set(msg, 0);
}

void app_phone_init(void) {
    eos_cell_set_handler(EOS_CELL_MTYPE_VOICE, cell_voice_handler);

    eos_i2s_mic_init(mic_arr, ABUF_SIZE);
    eos_i2s_mic_set_wm(MIC_WM);
    eos_i2s_mic_set_handler(handle_mic);
    eos_i2s_spk_init(spk_arr, ABUF_SIZE);
    eos_net_acquire_for_evt(EOS_EVT_I2S | EOS_I2S_ETYPE_MIC, 1);
}

unsigned char app_phone_state_get(void) {
    return voice_state;
}
