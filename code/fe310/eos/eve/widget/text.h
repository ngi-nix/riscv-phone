#include <stdint.h>

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
    EVETextCursor *cursor_f;
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

void eve_textw_init(EVETextWidget *widget, EVERect *g, EVEFont *font, char *text, uint64_t text_size, uint16_t *line, uint16_t line_size);
int eve_textw_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx, EVERect *focus);
uint8_t eve_textw_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);
void eve_textw_putc(void *_w, int c);
int eve_textw_update(EVETextWidget *widget, uint16_t line);
void eve_textw_cursor_update(EVETextWidget *widget, EVETextCursor *cursor);
void eve_textw_cursor_set(EVETextWidget *widget, EVETextCursor *cursor, uint8_t tag, int16_t x);
void eve_textw_cursor_clear(EVETextCursor *cursor);
