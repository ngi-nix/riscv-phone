#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <timer.h>
#include <power.h>

#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_font.h>

#include <eve/screen/window.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/app_root.h>

#include <prci_driver.h>

#include "status.h"
#include "cell_dev.h"
#include "cell_pdp.h"
#include "phone.h"
#include "modem.h"
#include "wifi.h"
#include "cam.h"
#include "fs.h"
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
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.title = "Camera",
            .widget.spec.page.constructor = app_cam
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.title = "File system",
            .widget.spec.page.constructor = app_fs
        },
        {
            .widget.type = EVE_WIDGET_TYPE_PAGE,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.page.title = "Test",
            .widget.spec.page.constructor = app_test
        },
    };

    EVEForm *form = eve_form_create(window, stack, spec, 7, NULL, NULL, NULL);
}

int main() {
    uint8_t wakeup_cause = eos_power_wakeup_cause();
    int rst = (wakeup_cause == EVE_INIT_RST);

    printf("\nREADY.\n");
    printf("FREQ:%lu\n", PRCI_get_cpu_freq());

    eos_init();

    app_root_init(app_home_page, 0x20);
    app_status_init();
    app_phone_init();
    app_wifi_init();
    app_cell_dev_init();
    app_cell_pdp_init();
    app_fs_init();
    eos_evtq_loop();
}
