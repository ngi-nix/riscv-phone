#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <i2c.h>
#include <i2c/bq25895.h>
#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_font.h>

#include <eve/screen/window.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/app_root.h>

#include <board.h>

#include "test.h"

#include <stdio.h>

int app_test_uievt(EVEForm *form, uint16_t evt, void *param) {
    uint8_t data = 0;
    int ret = 0, i;

    switch (evt) {
        case EVE_UIEVT_PAGE_TOUCH:
            printf("PAGE TOUCH\n");
            printf("BQ25895:\n");
            eos_i2c_start(400000);
            for (i=0; i<0x15; i++) {
                ret = eos_i2c_read8(BQ25895_ADDR, i, &data);
                if (!ret) printf("REG%02x: %02x\n", i, data);
            }
            eos_i2c_stop();
            break;

        default:
            ret = eve_form_uievt(form, evt, param);
            break;
    }
    return ret;
}

void app_test(EVEWindow *window, EVEViewStack *stack) {
    EVEWidgetSpec spec[] = {
        {
            .widget.type = EVE_WIDGET_TYPE_SPACER,
            .widget.g.h = 1,
        },
    };
    EVEForm *form = eve_form_create(window, stack, spec, 1, app_test_uievt, NULL, app_test_close);
}

void app_test_close(EVEForm *form) {
    eve_form_destroy(form);
}
