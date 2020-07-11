#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"
#include "kbdwin.h"

static int kbdwin_touch(EVEView *v, uint8_t tag0, int touch_idx) {
    EVEKbdView *k_view = (EVEKbdView *)v;

    return eve_kbd_touch(&k_view->kbd, tag0, touch_idx);
}

static uint8_t kbdwin_draw(EVEView *v, uint8_t tag0) {
    EVEKbdView *k_view = (EVEKbdView *)v;

    eve_kbd_draw(&k_view->kbd);
    return tag0;
}

void eve_kbdwin_init(EVEKbdWin *kbd_win, EVEScreen *screen) {
    EVEKbd *kbd = &kbd_win->view.kbd;

    kbd_win->view.v.touch = kbdwin_touch;
    kbd_win->view.v.draw = kbdwin_draw;
    eve_kbd_init(kbd, NULL, screen->mem_next, &screen->mem_next);
    eve_window_init(&kbd_win->win, &kbd->g, &kbd_win->view.v, screen);
}

void eve_kbdwin_append(EVEKbdWin *kbd_win) {
    EVEKbd *kbd = &kbd_win->view.kbd;
    EVEWindow *window = &kbd_win->win;

    eve_screen_set_kbd(window->screen, kbd);
    eve_window_append(window);
}
