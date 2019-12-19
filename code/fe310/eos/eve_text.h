#include <stdint.h>

typedef struct EVETextBox {
    uint16_t buf_addr;
    int buf_line_h;
    int w;
    int h;
    int x;
    int y;
    int buf_idx;
    int line_idx;
    uint16_t dl_offset;
    uint8_t bitmap_handle;
} EVETextBox;

void eos_eve_text_init(EVETextBox *box, uint16_t buf_addr, int buf_line_h, int w, int h, int x, int y, uint8_t bitmap_handle);
void eos_eve_text_update(EVETextBox *box);
void eos_eve_text_draw(EVETextBox *box);
void eos_eve_text_putc(EVETextBox *box, int c);
void eos_eve_text_newline(EVETextBox *box);