#include <stdint.h>

struct EVEPage;
struct EVEWidget;

typedef void (*eve_page_open_t) (struct EVEPage *, struct EVEPage *);
typedef void (*eve_page_close_t) (struct EVEPage *);
typedef void (*eve_page_evt_handler_t) (struct EVEPage *, struct EVEWidget *, EVETouch *, uint16_t, uint8_t, int);

typedef struct EVEPage {
    EVEView v;
    int16_t win_x;
    int16_t win_y;
    eve_page_open_t open;
    eve_page_close_t close;
    eve_page_evt_handler_t handle_evt;
    EVEWindow *window;
} EVEPage;

void eve_page_init(EVEPage *page, eve_view_touch_t touch, eve_view_draw_t draw, eve_page_open_t open, eve_page_close_t close, EVEWindow *window);
void eve_page_focus(EVEPage *page, EVERect *focus);
int eve_page_widget_visible(EVEPage *page, struct EVEWidget *widget);
void eve_page_handle_evt(EVEPage *page, struct EVEWidget *widget, EVETouch *touch, uint16_t evt, uint8_t tag0, int touch_idx);
