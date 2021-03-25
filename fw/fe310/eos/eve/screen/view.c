#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "window.h"

void eve_view_init(EVEView *view, EVEWindow *window, eve_view_draw_t draw, eve_view_touch_t touch, eve_view_uievt_t uievt, void *param) {
    view->draw = draw;
    view->touch = touch;
    view->uievt = uievt;
    view->param = param;
    view->window = window;
    view->color_bg = 0x000000;
    view->color_fg = 0xffffff;
    window->view = view;
}

void eve_view_set_color_bg(EVEView *view, uint8_t r, uint8_t g, uint8_t b) {
    view->color_bg = (r << 16) | (g << 8) | b;
}

void eve_view_set_color_fg(EVEView *view, uint8_t r, uint8_t g, uint8_t b) {
    view->color_fg = (r << 16) | (g << 8) | b;
}

uint8_t eve_view_clear(EVEView *view, uint8_t tag0) {
    eve_cmd_dl(CLEAR_COLOR_RGBC(view->color_bg));
    eve_cmd_dl(COLOR_RGBC(view->color_fg));
    view->tag = tag0;
    if (tag0 != EVE_TAG_NOTAG) {
        eve_cmd_dl(CLEAR_TAG(tag0));
        tag0++;
    }
    eve_cmd_dl(CLEAR(1,1,1));
    return tag0;
}

void eve_view_stack_init(EVEViewStack *stack) {
    memset(stack, 0, sizeof(EVEViewStack));
}

void eve_view_create(EVEWindow *window, EVEViewStack *stack, eve_view_constructor_t constructor) {
    if (stack->level < EVE_VIEW_SIZE_STACK - 1) {
        stack->constructor[stack->level] = constructor;
        stack->level++;
        constructor(window, stack);
    }
}

void eve_view_destroy(EVEWindow *window, EVEViewStack *stack) {
    if (stack->level > 1) {
        eve_view_constructor_t constructor;

        stack->level--;
        constructor = stack->constructor[stack->level - 1];
        constructor(window, stack);
    }
}

void eve_view_uievt_push(EVEView *view, uint16_t evt, void *param) {
    if (view->uievt) view->uievt(view, evt, param);
}