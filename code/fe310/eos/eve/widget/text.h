#include <stdint.h>

#include "widget.h"
#include "screen.h"
#include "font.h"

typedef struct EVETextCursor {
    char on;
    uint16_t x;
    uint16_t line;
    uint16_t ch;
} EVETextCursor;

typedef struct EVETextWidget {
    EVEWidget w;
    char *text;
    uint16_t text_size;
    uint16_t text_len;
    uint16_t *line;
    uint16_t line_size;
    uint16_t line_len;
    EVEFont *font;
    EVETextCursor cursor1;
    EVETextCursor cursor2;
    uint16_t line0;
    uint8_t tag0;
    uint8_t tagN;
    struct {
        EVETextCursor *cursor;
        short dx;
        short dl;
        char mode;
    } track;
} EVETextWidget;

void eve_textw_init(EVETextWidget *widget, uint16_t x, uint16_t y, uint16_t w, uint16_t h, char *text, uint64_t text_size, uint16_t *line, uint16_t line_size, EVEFont *font);
int eve_textw_touch(EVEWidget *_widget, EVEScreen *screen, uint8_t tag0, int touch_idx);
uint8_t eve_textw_draw(EVEWidget *_widget, EVEScreen *screen, uint8_t tag0, char active);
void eve_textw_putc(void *_w, int c);
int eve_textw_update(EVETextWidget *widget, uint16_t line);
void eve_textw_cursor_update(EVETextWidget *widget, EVETextCursor *cursor);
void eve_textw_cursor_set(EVETextWidget *widget, EVETextCursor *cursor, uint8_t tag, uint16_t x);
void eve_textw_cursor_clear(EVETextCursor *cursor);