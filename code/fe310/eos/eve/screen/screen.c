#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"

int eve_screen_init(EVEScreen *screen, uint16_t w, uint16_t h) {
    memset(screen, 0, sizeof(EVEScreen));
    screen->w = w;
    screen->h = h;
}

void eve_screen_set_kbd(EVEScreen *screen, EVEKbd *kbd) {
    screen->kbd = kbd;
}

EVEKbd *eve_screen_get_kbd(EVEScreen *screen) {
    return screen->kbd;
}

void eve_screen_show_kbd(EVEScreen *screen) {
    if (screen->kbd && screen->kbd_win) screen->kbd_win->g.y = screen->h - screen->kbd->g.h;
}

void eve_screen_hide_kbd(EVEScreen *screen) {
    if (screen->kbd && screen->kbd_win) screen->kbd_win->g.y = screen->h;
}

int eve_screen_win_insert(EVEScreen *screen, EVEWindow *window, int idx) {
    if (idx) {
        int i;
        EVEWindow *h = screen->win_head;

        for (i=1; i<idx; i++) {
            h = h->next;
            if (h == NULL) return EVE_ERR;
        }
        window->next = h->next;
        h->next = window;
    } else {
        window->next = screen->win_head;
        screen->win_head = window;
    }
    if (window->next == NULL) screen->win_tail = window;
    return EVE_OK;
}

int eve_screen_win_remove(EVEScreen *screen, EVEWindow *window) {
    EVEWindow *h = screen->win_head;
    if (h == NULL) return EVE_ERR;
    if (h == window) {
        screen->win_head = window->next;
        if (screen->win_head == NULL) screen->win_tail = NULL;
    } else {
        while (h->next && (h->next != window)) h = h->next;
        if (h->next == NULL) return EVE_ERR;

        h->next = window->next;
        if (h->next == NULL) screen->win_tail = h;
    }
}

void eve_screen_win_append(EVEScreen *screen, EVEWindow *window) {
    screen->win_tail->next = window;
    screen->win_tail = window;
}

void eve_screen_handle_touch(EVEScreen *screen, uint8_t tag0, int touch_idx) {
    EVEWindow *w;
    int a;
    uint8_t tagN = 0x80;

    eve_touch_clear_opt();

    w = screen->win_head;
    while(w) {
        if (eve_window_visible(w)) a = w->view->touch(w->view, tag0, touch_idx);
        w = w->next;
    }

    w = screen->win_head;
    while(w) {
        if (eve_window_visible(w)) tagN = w->view->draw(w->view, tagN);
        w = w->next;
    }
}
