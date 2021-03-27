#include <stdint.h>

#define EVE_PAGE_KBDCH_CLOSE        0x1a

#define EVE_PAGE_OPT_SCROLL_X       0x01
#define EVE_PAGE_OPT_SCROLL_Y       0x02
#define EVE_PAGE_OPT_SCROLL_BACK    0x04
#define EVE_PAGE_OPT_SCROLL_XY      0x08
#define EVE_PAGE_OPT_TRACK_EXT_X    0x10
#define EVE_PAGE_OPT_TRACK_EXT_Y    0x20
#define EVE_PAGE_OPT_TRACK_EXT_XY  (EVE_PAGE_OPT_TRACK_EXT_X | EVE_PAGE_OPT_TRACK_EXT_Y)

struct EVEWidget;
struct EVEPage;

typedef void (*eve_page_destructor_t) (struct EVEPage *);

typedef struct EVEPage {
    EVEView v;
    EVERect g;
    int16_t x0;
    int16_t y0;
    EVEViewStack *stack;
    eve_page_destructor_t destructor;
    struct EVEWidget *widget;
    uint16_t widget_size;
    struct EVEWidget *widget_f;
    EVEPhyLHO lho;
    uint64_t lho_t0;
    uint8_t track_mode;
    uint8_t opt;
} EVEPage;

void eve_page_init(EVEPage *page, EVEWindow *window, EVEViewStack *stack, struct EVEWidget *widget, uint16_t widget_size, uint8_t opt,eve_view_draw_t draw, eve_view_touch_t touch, eve_view_uievt_t uievt, eve_page_destructor_t destructor);
void eve_page_update(EVEPage *page, struct EVEWidget *widget, uint16_t widget_size);
void eve_page_open(EVEPage *parent, eve_view_constructor_t constructor);
void eve_page_close(EVEPage *page);

/* Screen to page coordinates */
int16_t eve_page_x(EVEPage *page, int16_t x);
int16_t eve_page_y(EVEPage *page, int16_t y);
/* Page to window coordinates */
int16_t eve_page_win_x(EVEPage *page, int16_t x);
int16_t eve_page_win_y(EVEPage *page, int16_t y);
/* Page to screen coordinates */
int16_t eve_page_scr_x(EVEPage *page, int16_t x);
int16_t eve_page_scr_y(EVEPage *page, int16_t y);
int eve_page_rect_visible(EVEPage *page, EVERect *g);

void eve_page_focus(EVEPage *page, EVERect *rect);
void eve_page_focus_widget(EVEPage *page, struct EVEWidget *widget, EVERect *rect);
struct EVEWidget *eve_page_focus_widget_get(EVEPage *page);
struct EVEWidget *eve_page_widget(EVEPage *page, uint16_t idx);

uint8_t eve_page_draw(EVEView *view, uint8_t tag0);
int eve_page_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0);
