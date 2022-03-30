#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <net.h>
#include <wifi.h>

#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_font.h>

#include <eve/screen/window.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include "app/app_root.h"
#include "app/app_status.h"

#include "wifi.h"

static void wifi_scan_handler(unsigned char type, unsigned char *buffer, uint16_t size) {
    EVEWindowRoot *root = app_root();
    EVEWindow *window = eve_window_search(&root->w, "main");
    EVEForm *form = (EVEForm *)window->view;
    EVESelectWidget *select = (EVESelectWidget *)eve_page_widget(&form->p, 0);

    eve_selectw_option_set(select, buffer + 1, size - 1);
    eos_net_free(buffer, 0);

    app_root_refresh();
}

static void wifi_connect_handler(unsigned char type, unsigned char *buffer, uint16_t size) {
    app_status_msg_set("WiFi connected", 1);
    eos_net_free(buffer, 0);
}

static void wifi_disconnect_handler(unsigned char type, unsigned char *buffer, uint16_t size) {
    app_status_msg_set("WiFi disconnected", 1);
    eos_net_free(buffer, 0);
}

void app_wifi(EVEWindow *window, EVEViewStack *stack) {
    EVEWidgetSpec spec[] = {
        {
            .label.g.w = APP_SCREEN_W,
            .label.title = "Select network:",

            .widget.type = EVE_WIDGET_TYPE_SELECT,
            .widget.g.w = APP_SCREEN_W,
            .widget.spec.select.option_size = 1500,
        },
        {
            .widget.type = EVE_WIDGET_TYPE_SPACER,
            .widget.g.w = APP_SCREEN_W,
            .widget.g.h = 50,
        },
        {
            .label.title = "Password:",

            .widget.type = EVE_WIDGET_TYPE_STR,
            .widget.spec.str.str_size = 128,
        },
    };

    EVEForm *form = eve_form_create(window, stack, spec, 3, NULL, app_wifi_action, app_wifi_close);
    eos_wifi_scan(NULL);
}

void app_wifi_action(EVEForm *form) {
    EVESelectWidget *sel = (EVESelectWidget *)eve_page_widget(&form->p, 0);
    EVEStrWidget *str = (EVEStrWidget *)eve_page_widget(&form->p, 2);
    char *ssid = eve_selectw_option_get_select(sel);

    if (ssid) {
        eos_wifi_auth(ssid, str->str, NULL);
        eos_wifi_connect(NULL);
    }
}

void app_wifi_close(EVEForm *form) {
    eve_form_destroy(form);
}

void app_wifi_init(void) {
    eos_wifi_set_handler(EOS_WIFI_MTYPE_SCAN, wifi_scan_handler);
    eos_wifi_set_handler(EOS_WIFI_MTYPE_CONNECT, wifi_connect_handler);
    eos_wifi_set_handler(EOS_WIFI_MTYPE_DISCONNECT, wifi_disconnect_handler);
}
