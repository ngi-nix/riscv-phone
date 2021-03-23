#include <stdlib.h>

#include "net.h"
#include "unicode.h"

#include "eve/eve.h"
#include "eve/eve_kbd.h"
#include "eve/eve_font.h"

#include "eve/screen/screen.h"
#include "eve/screen/window.h"
#include "eve/screen/view.h"
#include "eve/screen/page.h"
#include "eve/screen/form.h"

#include "eve/widget/widgets.h"

#include "app_status.h"
#include "app_screen.h"

static EVEKbd kbd;
static EVEScreen screen;
static EVEWindow win_status;
static EVEWindow win_main;
static EVEWindow win_kbd;
static EVEView view_kbd;
static EVEView view_status;
static EVEViewStack view_stack;

EVEScreen *app_screen(void) {
    return &screen;
}

static int kbd_touch(EVEView *v, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    EVEKbd *kbd = v->param;

    return eve_kbd_touch(kbd, touch, evt, tag0);
}

static uint8_t kbd_draw(EVEView *v, uint8_t tag0) {
    EVEKbd *kbd = v->param;

    eve_kbd_draw(kbd);
    return tag0;
}

void app_screen_refresh(void) {
    eve_spi_start();
    eve_screen_draw(app_screen());
    eve_spi_stop();
}

void app_screen_init(eve_view_constructor_t home_page) {
    EVERect g;

    eve_spi_start();

    eve_brightness(0x40);

    eve_screen_init(&screen, APP_SCREEN_W, APP_SCREEN_H);

    eve_kbd_init(&kbd, NULL, screen.mem_next, &screen.mem_next);
    eve_screen_set_kbd(&screen, &kbd);

    g.x = 0;
    g.y = 0;
    g.w = APP_SCREEN_W;
    g.h = APP_STATUS_H;
    eve_window_init(&win_status, &g, &screen, "status");
    eve_view_init(&view_status, &win_status, app_status_touch, app_status_draw, NULL);

    g.x = 0;
    g.y = APP_STATUS_H;
    g.w = APP_SCREEN_W;
    g.h = APP_SCREEN_H - APP_STATUS_H;
    eve_window_init(&win_main, &g, &screen, "main");

    eve_window_init(&win_kbd, &kbd.g, &screen, "kbd");
    eve_view_init(&view_kbd, &win_kbd, kbd_touch, kbd_draw, &kbd);

    eve_view_stack_init(&view_stack);
    eve_view_create(&win_main, &view_stack, home_page);

    eve_window_append(&win_status);
    eve_window_append(&win_main);
    eve_window_append(&win_kbd);

    eve_screen_hide_kbd(&screen);
    eve_screen_draw(&screen);

    eve_spi_stop();

    eos_net_acquire_for_evt(EOS_EVT_UI | EVE_ETYPE_INTR, 1);
}
