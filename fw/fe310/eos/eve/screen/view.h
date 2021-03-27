#include <stdint.h>

#include "uievt.h"

#define EVE_VIEW_SIZE_STACK             16

struct EVEView;
struct EVEViewStack;
struct EVEWindow;

typedef uint8_t (*eve_view_draw_t) (struct EVEView *, uint8_t);
typedef int (*eve_view_touch_t) (struct EVEView *, EVETouch *, uint16_t, uint8_t);
typedef int (*eve_view_uievt_t) (struct EVEView *, uint16_t, void *);
typedef void (*eve_view_constructor_t) (struct EVEWindow *window, struct EVEViewStack *);

typedef struct EVEView {
    eve_view_draw_t draw;
    eve_view_touch_t touch;
    eve_view_uievt_t uievt;
    struct EVEWindow *window;
    void *param;
    uint32_t color_bg;
    uint32_t color_fg;
    uint8_t tag;
} EVEView;

typedef struct EVEViewStack {
    eve_view_constructor_t constructor[EVE_VIEW_SIZE_STACK];
    uint8_t level;
} EVEViewStack;

void eve_view_init(EVEView *view, struct EVEWindow *window, eve_view_draw_t draw, eve_view_touch_t touch, eve_view_uievt_t uievt, void *param);
void eve_view_set_color_bg(EVEView *view, uint8_t r, uint8_t g, uint8_t b);
void eve_view_set_color_fg(EVEView *view, uint8_t r, uint8_t g, uint8_t b);
uint8_t eve_view_clear(EVEView *view, uint8_t tag0, uint8_t tag_opt);

void eve_view_stack_init(EVEViewStack *stack);
void eve_view_create(struct EVEWindow *window, EVEViewStack *stack, eve_view_constructor_t constructor);
void eve_view_destroy(struct EVEWindow *window, EVEViewStack *stack);

void eve_view_uievt_push(EVEView *view, uint16_t evt, void *param);
int eve_view_uievt_tpush(EVEView *view, uint16_t evt, EVETouch *touch, uint16_t t_evt, uint8_t tag0);
