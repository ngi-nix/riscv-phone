#include <stdint.h>

struct EVEPage;
struct EVEWidget;

typedef void (*eve_page_open_t) (struct EVEPage *, struct EVEPage *);
typedef void (*eve_page_close_t) (struct EVEPage *);
typedef void (*eve_page_evt_handler_t) (struct EVEPage *, struct EVEWidget *, EVETouch *, uint16_t, uint8_t, int);

typedef struct EVEPage {
    EVECanvas c;
    int16_t win_x;
    int16_t win_y;
    eve_page_evt_handler_t handle_evt;
    eve_page_open_t open;
    eve_page_close_t close;
    EVETile *tile;
} EVEPage;

typedef struct EVEPageFocus {
    struct EVEWidget *w;
    EVEWindow f;
} EVEPageFocus;

void eve_page_init(EVEPage *page, EVETile *tile, eve_canvas_touch_t touch, eve_canvas_draw_t draw, eve_page_open_t open, eve_page_close_t close);
void eve_page_focus(EVEPage *page, EVEPageFocus *focus);
void eve_page_widget_focus(EVEPageFocus *focus, struct EVEWidget *widget);
int eve_page_widget_visible(EVEPage *page, struct EVEWidget *widget);
void eve_page_handle_evt(EVEPage *page, struct EVEWidget *widget, EVETouch *touch, uint16_t evt, uint8_t tag0, int touch_idx);
