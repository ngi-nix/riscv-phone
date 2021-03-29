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

#include "status.h"
#include "cell_dev.h"
#include "cell_pdp.h"
#include "phone.h"
#include "modem.h"
#include "wifi.h"
#include "test.h"

void app_home_page(EVEWindow *window, EVEViewStack *stack) {
    EVEWidgetSpec spec[] = {
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.title = "Phone",
            .widget.spec.page.constructor = app_phone
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.title = "WiFi",
            .widget.spec.page.constructor = app_wifi
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.title = "Cellular data",
            .widget.spec.page.constructor = app_cell_pdp
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.title = "Modem",
            .widget.spec.page.constructor = app_modem
        },
    };

    EVEForm *form = eve_form_create(window, stack, spec, 4, NULL, NULL, NULL);
}

int main() {
    printf("\nREADY.\n");

    eos_init();

    app_root_init(app_home_page);
    app_status_init();
    app_phone_init();
    app_wifi_init();
    app_cell_dev_init();
    app_cell_pdp_init();

    eos_evtq_loop();
}
