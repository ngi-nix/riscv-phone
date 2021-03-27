#include <stdint.h>

#include "view.h"

struct EVEWindowRoot;

typedef struct EVEWindow {
    EVERect g;
    char *name;
    EVEView *view;
    struct EVEWindowRoot *root;
    struct EVEWindow *parent;
    struct EVEWindow *next;
    struct EVEWindow *prev;
    struct EVEWindow *child_head;
    struct EVEWindow *child_tail;
} EVEWindow;

typedef struct EVEWindowKbd {
    EVEWindow w;
    EVEView v;
    EVEKbd *kbd;
} EVEWindowKbd;

typedef struct EVEWindowRoot {
    EVEWindow w;
    uint32_t mem_next;
    EVEFont *font;
    EVEWindowKbd *win_kbd;
    EVEWindow *win_scroll;
    uint8_t tag0;
} EVEWindowRoot;

void eve_window_init(EVEWindow *window, EVERect *g, EVEWindow *parent, char *name);
void eve_window_init_root(EVEWindowRoot *root, EVERect *g, char *name, EVEFont *font);
void eve_window_init_kbd(EVEWindowKbd *win_kbd, EVERect *g, EVEWindowRoot *root, char *name, EVEKbd *kbd);
void eve_window_set_parent(EVEWindow *window, EVEWindow *parent);

int eve_window_visible(EVEWindow *window);
void eve_window_visible_g(EVEWindow *window, EVERect *g);

void eve_window_append(EVEWindow *window);
void eve_window_insert_above(EVEWindow *window, EVEWindow *win_prev);
void eve_window_insert_below(EVEWindow *window, EVEWindow *win_next);
void eve_window_remove(EVEWindow *window);
EVEWindow *eve_window_search(EVEWindow *window, char *name);

uint8_t eve_window_draw(EVEWindow *window, uint8_t tag0);
int eve_window_touch(EVEWindow *window, EVETouch *touch, uint16_t evt, uint8_t tag0);
void eve_window_root_draw(EVEWindowRoot *root);
void eve_window_root_touch(EVETouch *touch, uint16_t evt, uint8_t tag0, void *win);

EVEKbd *eve_window_kbd(EVEWindow *window);
void eve_window_kbd_attach(EVEWindow *window);
void eve_window_kbd_detach(EVEWindow *window);
EVEFont *eve_window_font(EVEWindow *window);

EVEWindow *eve_window_scroll(EVEWindowRoot *root, uint8_t *tag);
void eve_window_scroll_start(EVEWindow *window, uint8_t tag);
void eve_window_scroll_stop(EVEWindow *window);
