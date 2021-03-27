#include <stdint.h>

typedef struct EVETextCursor {
    char on;
    uint16_t x;
    uint16_t line;
    uint16_t ch;
} EVETextCursor;

typedef struct EVETextWidget {
    EVEWidget w;
    EVEFont *font;
    utf8_t *text;
    uint16_t text_size;
    uint16_t text_len;
    uint16_t *line;
    uint16_t line_size;
    uint16_t line_len;
    EVETextCursor cursor1;
    EVETextCursor cursor2;
    uint16_t line0;
    struct {
        EVETextCursor *cursor;
        short dx;
        short dl;
    } track;
} EVETextWidget;

typedef struct EVETextSpec {
    EVEFont *font;
    uint16_t text_size;
    uint16_t line_size;
} EVETextSpec;

int eve_textw_create(EVETextWidget *widget, EVERect *g, EVEPage *page, EVETextSpec *spec);
void eve_textw_init(EVETextWidget *widget, EVERect *g, EVEPage *page, EVEFont *font, utf8_t *text, uint16_t text_size, uint16_t *line, uint16_t line_size);
void eve_textw_destroy(EVETextWidget *widget);

uint8_t eve_textw_draw(EVEWidget *_widget, uint8_t tag0);
int eve_textw_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt);
void eve_textw_putc(void *_w, int c);

uint16_t eve_textw_text_update(EVETextWidget *widget, uint16_t line, int uievt);
void eve_textw_cursor_update(EVETextWidget *widget, EVETextCursor *cursor);
void eve_textw_cursor_set(EVETextWidget *widget, EVETextCursor *cursor, uint8_t tag, int16_t x);
void eve_textw_cursor_clear(EVETextWidget *widget, EVETextCursor *cursor);
