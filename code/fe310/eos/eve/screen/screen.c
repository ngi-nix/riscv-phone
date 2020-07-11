#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"
#include "kbdwin.h"

int eve_screen_init(EVEScreen *screen, uint16_t w, uint16_t h) {
    memset(screen, 0, sizeof(EVEScreen));
    screen->w = w;
    screen->h = h;
    screen->mem_next = EVE_RAM_G;
    eve_touch_set_handler(eve_screen_handle_touch, screen);
}

void eve_screen_set_kbd(EVEScreen *screen, EVEKbd *kbd) {
    screen->kbd = kbd;
}

EVEKbd *eve_screen_get_kbd(EVEScreen *screen) {
    return screen->kbd;
}

void eve_screen_show_kbd(EVEScreen *screen) {
    EVEWindow *win = screen->win_tail;
    EVEKbd *kbd = eve_screen_get_kbd(screen);

    if (win) win->g.y = screen->h - kbd->g.h;
}

void eve_screen_hide_kbd(EVEScreen *screen) {
    EVEWindow *win = screen->win_tail;

    if (win) win->g.y = screen->h;
}

void eve_screen_draw(EVEScreen *screen) {
    EVEWindow *win;
    uint8_t tagN = 0x80;

    eve_cmd_burst_start();
	eve_cmd_dl(CMD_DLSTART);

    win = screen->win_head;
    while (win) {
        if (eve_window_visible(win)) {
            int16_t x = win->g.x;
            int16_t y = win->g.y;
            uint16_t w = win->g.w;
            uint16_t h = win->g.h;

            if (x < 0) {
                w += x;
                x = 0;
            }
            if (y < 0) {
                h += y;
                y = 0;
            }
            if (x + w > screen->w) w = screen->w - x;
            if (y + h > screen->h) h = screen->h - y;
            win->tag = tagN;

            if (tagN != EVE_TAG_NOTAG) {
                eve_cmd_dl(CLEAR_TAG(tagN));
                tagN++;
            }
            eve_cmd_dl(CLEAR_COLOR_RGBC(win->color_bg));
            eve_cmd_dl(SCISSOR_XY(x, y));
            eve_cmd_dl(SCISSOR_SIZE(w, h));
            eve_cmd_dl(CLEAR(1,1,1));
            eve_cmd_dl(COLOR_RGBC(win->color_fg));
            tagN = win->view->draw(win->view, tagN);
        }
        win = win->next;
    }

    eve_cmd_dl(DISPLAY());
	eve_cmd_dl(CMD_SWAP);
    eve_cmd_burst_end();
    eve_cmd_exec(1);
}

void eve_screen_handle_touch(void *s, uint8_t tag0, int touch_idx) {
    EVEScreen *screen = s;
    EVEWindow *win;

    eve_touch_clear_opt();

    if (touch_idx >= 0) {
        win = screen->win_tail;
        while (win) {
            if (eve_window_visible(win)) {
                int a = win->view->touch(win->view, tag0, touch_idx);
            }
            win = win->prev;
        }
    }

    eve_screen_draw(screen);
}
