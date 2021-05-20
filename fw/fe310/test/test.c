#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
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
    int ret = 0;

    switch (evt) {
        case EVE_UIEVT_PAGE_TOUCH:
            printf("PAGE TOUCH\n");
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
