#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "window.h"

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))

void eve_window_init(EVEWindow *window, EVERect *g, EVEWindow *parent, char *name) {
    memset(window, 0, sizeof(EVEWindow));

    if (g) window->g = *g;
    window->root = parent ? parent->root : NULL;
    window->parent = parent;
    window->name = name;
}

void eve_window_init_root(EVEWindowRoot *root, EVERect *g, char *name, EVEFont *font) {
    EVEWindow *_window = &root->w;

    eve_window_init(_window, g, NULL, name);
    _window->root = root;
    root->mem_next = EVE_RAM_G;
    root->font = font;
    root->win_kbd = NULL;
    root->tag0 = EVE_NOTAG;
    eve_touch_set_handler(eve_window_root_touch, root);
}

static uint8_t kbd_draw(EVEView *view, uint8_t tag0) {
    EVEKbd *kbd = view->param;

    tag0 = eve_view_clear(view, tag0, 0);

    eve_kbd_draw(kbd);
    return tag0;
}

static int kbd_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    EVEKbd *kbd = view->param;

    return eve_kbd_touch(kbd, touch, evt, tag0);
}

void eve_window_init_kbd(EVEWindowKbd *win_kbd, EVERect *g, EVEWindowRoot *root, char *name, EVEKbd *kbd) {
    EVEWindow *_window = &win_kbd->w;

    eve_window_init(_window, g, NULL, name);
    _window->root = root;
    win_kbd->kbd = kbd;
    root->win_kbd = win_kbd;
    eve_view_init(&win_kbd->v, _window, kbd_draw, kbd_touch, NULL, kbd);
}

void eve_window_set_parent(EVEWindow *window, EVEWindow *parent) {
    window->parent = parent;
    window->root = parent->root;
}

int eve_window_visible(EVEWindow *window) {
    if (window->g.x >= window->root->w.g.w) return 0;
    if (window->g.y >= window->root->w.g.h) return 0;
    if ((window->g.x + window->g.w) <= 0) return 0;
    if ((window->g.y + window->g.h) <= 0) return 0;
    return 1;
}

static void _window_visible_g(EVEWindow *w, EVERect *g) {
    while (w) {
        if (eve_window_visible(w)) {
            if (w->g.x > g->x) g->w = MIN(g->w, w->g.x - g->x);
            if (w->g.y > g->y) g->h = MIN(g->h, w->g.y - g->y);
            if (w->g.x + w->g.w < g->x + g->w) {
                uint16_t x0 = g->w - MIN(g->w, (g->x + g->w) - (w->g.x + w->g.w));
                g->x += x0;
                g->w -= x0;
            }
            if (w->g.y + w->g.h < g->y + g->h) {
                uint16_t y0 = g->h - MIN(g->h, (g->y + g->h) - (w->g.y + w->g.h));
                g->y += y0;
                g->h -= y0;
            }
        }
        if (w->child_head) _window_visible_g(w->child_head, g);
        w = w->next;
    }
}

void eve_window_visible_g(EVEWindow *window, EVERect *g) {
    *g = window->g;
    if (window->child_head) _window_visible_g(window->child_head, g);
    _window_visible_g(window->next, g);
}

void eve_window_append(EVEWindow *window) {
    EVEWindow *parent = window->parent;

    window->prev = parent->child_tail;
    window->next = NULL;
    if (parent->child_tail) {
        parent->child_tail->next = window;
    } else {
        parent->child_head = window;
    }
    parent->child_tail = window;
}

void eve_window_insert_above(EVEWindow *window, EVEWindow *win_prev) {
    EVEWindow *parent = window->parent;

    window->prev = win_prev;
    window->next = win_prev->next;

    if (window->next) {
        window->next->prev = window;
    } else {
        parent->child_tail = window;
    }
    win_prev->next = window;
}

void eve_window_insert_below(EVEWindow *window, EVEWindow *win_next) {
    EVEWindow *parent = window->parent;

    window->prev = win_next->prev;
    window->next = win_next;

    win_next->prev = window;
    if (window->prev) {
        window->prev->next = window;
    } else {
        parent->child_head = window;
    }
}

void eve_window_remove(EVEWindow *window) {
    EVEWindow *parent = window->parent;

    if (window->prev) {
        window->prev->next = window->next;
    } else {
        parent->child_head = window->next;
    }
    if (window->next) {
        window->next->prev = window->prev;
    } else {
        parent->child_tail = window->prev;
    }
    window->parent = NULL;
    window->prev = NULL;
    window->next = NULL;
}

EVEWindow *eve_window_search(EVEWindow *window, char *name) {
    while (window) {
        if (window->name && (strcmp(name, window->name) == 0)) return window;
        if (window->child_head) {
            EVEWindow *ret = eve_window_search(window->child_head, name);
            if (ret) return ret;
        }
        window = window->next;
    }

    return NULL;
}

uint8_t eve_window_draw(EVEWindow *window, uint8_t tag0) {
    while (window) {
        if (eve_window_visible(window) && window->view) {
            int16_t s_x = window->g.x;
            int16_t s_y = window->g.y;
            uint16_t s_w = window->g.w;
            uint16_t s_h = window->g.h;

            if (s_x < 0) {
                s_w += s_x;
                s_x = 0;
            }
            if (s_y < 0) {
                s_h += s_y;
                s_y = 0;
            }
            if (s_x + s_w > window->root->w.g.w) s_w = window->root->w.g.w - s_x;
            if (s_y + s_h > window->root->w.g.h) s_h = window->root->w.g.h - s_y;
            eve_cmd_dl(SCISSOR_XY(s_x, s_y));
            eve_cmd_dl(SCISSOR_SIZE(s_w, s_h));
            tag0 = window->view->draw(window->view, tag0);
        }
        if (window->child_head) tag0 = eve_window_draw(window->child_head, tag0);
        window = window->next;
    }

    return tag0;
}

int eve_window_touch(EVEWindow *window, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    int ret = 0;

    while (window) {
        if (window->child_tail) {
            ret = eve_window_touch(window->child_tail, touch, evt, tag0);
            if (ret) return 1;
        }
        if (eve_window_visible(window) && window->view) {
            ret = window->view->touch(window->view, touch, evt, tag0);
            if (ret) return 1;
        }
        window = window->prev;
    }

    return 0;
}

void eve_window_root_draw(EVEWindowRoot *root) {
    uint8_t tag0 = 0x80;

    eve_cmd_burst_start();
	eve_cmd_dl(CMD_DLSTART);

    if (root->tag0 != EVE_NOTAG) tag0 = EVE_NOTAG;
    eve_window_draw(&root->w, tag0);

    eve_cmd_dl(DISPLAY());
	eve_cmd_dl(CMD_SWAP);
    eve_cmd_burst_end();
    eve_cmd_exec(1);
}

void eve_window_root_touch(EVETouch *touch, uint16_t evt, uint8_t tag0, void *win) {
    EVEWindowRoot *root = (EVEWindowRoot *)win;
    int ret;

    if (root->tag0 != EVE_NOTAG) tag0 = root->tag0;
    ret = eve_window_touch(&root->w, touch, evt, tag0);
    if (ret) {
        eve_touch_clear_opt();
        eve_window_root_draw(root);
    }
}

EVEKbd *eve_window_kbd(EVEWindow *window) {
    EVEWindowRoot *root = window->root;
    EVEWindowKbd *win_kbd = root->win_kbd;

    if (win_kbd) return win_kbd->kbd;
    return NULL;
}

void eve_window_kbd_attach(EVEWindow *window) {
    EVEWindowRoot *root = window->root;
    EVEWindowKbd *win_kbd = root->win_kbd;
    EVEKbd *kbd = win_kbd ? win_kbd->kbd : NULL;

    if (kbd) {
        eve_window_set_parent(&win_kbd->w, window);
        eve_window_append(&win_kbd->w);
    }
}

void eve_window_kbd_detach(EVEWindow *window) {
    EVEWindowRoot *root = window->root;
    EVEWindowKbd *win_kbd = root->win_kbd;
    EVEKbd *kbd = win_kbd ? win_kbd->kbd : NULL;

    if (kbd && win_kbd->w.parent) {
        eve_window_remove(&win_kbd->w);
        eve_kbd_close(kbd);
    }
}

EVEFont *eve_window_font(EVEWindow *window) {
    EVEWindowRoot *root = window->root;

    return root->font;
}

EVEWindow *eve_window_scroll(EVEWindowRoot *root, uint8_t *tag) {
    if (tag) *tag = root->tag0;
    return root->win_scroll;
}

void eve_window_scroll_start(EVEWindow *window, uint8_t tag) {
    EVEWindowRoot *root = window->root;

    root->win_scroll = window;
    root->tag0 = tag;
}

void eve_window_scroll_stop(EVEWindow *window) {
    EVEWindowRoot *root = window->root;

    root->win_scroll = NULL;
    root->tag0 = EVE_NOTAG;
}
