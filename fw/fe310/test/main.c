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

#include <eve/screen/screen.h>
#include <eve/screen/window.h>
#include <eve/screen/view.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/root.h>

#include "status.h"
#include "phone.h"
#include "wifi.h"
#include "cell_data.h"
#include "modem.h"

extern EVEFont *_app_font_default;

void app_home_page(EVEWindow *window, EVEViewStack *stack) {
    APPWidgetSpec spec[] = {
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.font = _app_font_default,
            .widget.spec.page.title = "Phone",
            .widget.spec.page.constructor = app_phone
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.font = _app_font_default,
            .widget.spec.page.title = "WiFi",
            .widget.spec.page.constructor = app_wifi
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.font = _app_font_default,
            .widget.spec.page.title = "Cellular data",
            .widget.spec.page.constructor = app_cell_data
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.font = _app_font_default,
            .widget.spec.page.title = "Modem",
            .widget.spec.page.constructor = app_modem
        },
    };

    EVEForm *form = app_form_create(window, stack, spec, 4, NULL, NULL);
}

int main() {
    printf("\nREADY.\n");

    eos_init();

    app_root_init(app_home_page);
    app_status_init();
    app_phone_init();
    app_wifi_init();
    app_cell_data_init();

    eos_evtq_loop();
}
