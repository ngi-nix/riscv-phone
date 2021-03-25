#include <stdlib.h>

#include "net.h"

#include "eve/eve.h"
#include "eve/eve_kbd.h"
#include "eve/eve_font.h"

#include "eve/screen/window.h"
#include "eve/screen/page.h"
#include "eve/screen/form.h"

#include "eve/widget/widgets.h"

#include "app_status.h"
#include "app_root.h"

#define KBD_X           0
#define KBD_Y           575
#define KBD_W           480
#define KBD_H           225

static EVEKbd kbd;
static EVEFont font;
static EVEWindowRoot win_root;
static EVEWindowKbd win_kbd;
static EVEWindow win_status;
static EVEWindow win_main;
static EVEView view_status;
static EVEViewStack view_stack;

EVEWindow *app_root(void) {
    return (EVEWindow *)&win_root;
}

void app_root_refresh(void) {
    eve_spi_start();
    eve_window_root_draw(app_root());
    eve_spi_stop();
}

void app_root_init(eve_view_constructor_t home_page) {
    EVERect g;

    eve_spi_start();

    eve_brightness(0x40);

    eve_font_init(&font, APP_FONT_HANDLE);

    g.x = 0;
    g.y = 0;
    g.w = APP_SCREEN_W;
    g.h = APP_SCREEN_H;
    eve_window_init_root(&win_root, &g, "root", &font);

    g.x = KBD_X;
    g.y = KBD_Y;
    g.w = KBD_W;
    g.h = KBD_H;
    eve_kbd_init(&kbd, &g, win_root.mem_next, &win_root.mem_next);
    eve_window_init_kbd(&win_kbd, &g, &win_root, "kbd", &kbd);

    g.x = 0;
    g.y = 0;
    g.w = APP_SCREEN_W;
    g.h = APP_STATUS_H;
    eve_window_init(&win_status, &g, (EVEWindow *)&win_root, "status");
    eve_view_init(&view_status, &win_status, app_status_draw, app_status_touch, NULL, NULL);

    g.x = 0;
    g.y = APP_STATUS_H;
    g.w = APP_SCREEN_W;
    g.h = APP_SCREEN_H - APP_STATUS_H;
    eve_window_init(&win_main, &g, (EVEWindow *)&win_root, "main");

    eve_view_stack_init(&view_stack);
    eve_view_create(&win_main, &view_stack, home_page);

    eve_window_append(&win_status);
    eve_window_append(&win_main);

    eve_window_root_draw((EVEWindow *)&win_root);

    eve_spi_stop();

    eos_net_acquire_for_evt(EOS_EVT_UI | EVE_ETYPE_INTR, 1);
}
