#include <stdlib.h>

#include "eve.h"
#include "eve_kbd.h"

#define KBD_X       0
#define KBD_Y       575
#define KBD_W       480
#define KBD_H       225

#define KEY_SPACERX 3
#define KEY_SPACERY 5
#define KEY_FONT    29
#define MOD_FONT    21

#define KEY_BS      0x08
#define KEY_RET     0x0a

#define FLAG_SHIFT  0x01
#define FLAG_CTRL   0x02
#define FLAG_FN     0x04

#define TAG_SHIFT   0x11
#define TAG_CTRL    0x12
#define TAG_FN      0x13

void eve_kbd_init(EVEKbd *kbd, uint32_t mem_addr, uint32_t *mem_next) {
    uint16_t mem_size;

    kbd->x = KBD_X;
    kbd->y = KBD_Y;
    kbd->w = KBD_W;
    kbd->h = KBD_H;
    kbd->mem_addr = mem_addr;
    kbd->key_modifier = 0;
    kbd->key_count = 0;
    kbd->key_down = 0;
    kbd->putc = NULL;
    kbd->param = NULL;

    kbd->active = 1;
    eve_write16(REG_CMD_DL, 0);
    eve_kbd_draw(kbd);
    eve_cmd_exec(1);
    mem_size = eve_read16(REG_CMD_DL);
    eve_cmd(CMD_MEMCPY, "www", mem_addr, EVE_RAM_DL, mem_size);
    eve_cmd_exec(1);
    kbd->active = 0;
    kbd->mem_size = mem_size;

    *mem_next = kbd->mem_addr + kbd->mem_size;
}

void eve_kbd_set_handler(EVEKbd *kbd, eve_kbd_input_handler_t putc, void *param) {
    kbd->putc = putc;
    kbd->param = param;
}

int eve_kbd_touch(EVEKbd *kbd, uint8_t tag0, int touch_idx) {
    EVETouch *t;
    uint16_t evt;

    t = eve_touch_evt(tag0, touch_idx, 1, 0x7e, &evt);
    if (t && evt) {
        if (evt & EVE_TOUCH_ETYPE_TAG) {
            uint8_t _tag = t->tag;

            if (_tag >= TAG_SHIFT && _tag <= TAG_FN) {
                if (touch_idx == 0) {
                    uint8_t f = (1 << (_tag - TAG_SHIFT));

                    kbd->key_modifier = f;
                    kbd->key_modifier_sticky &= f;
                    kbd->key_modifier_lock &= f;
                    if (kbd->key_modifier_lock & f) {
                        kbd->key_modifier_lock &= ~f;
                    } else if (kbd->key_modifier_sticky & f) {
                        kbd->key_modifier_sticky &= ~f;
                        kbd->key_modifier_lock |= f;
                    } else {
                        kbd->key_modifier_sticky |= f;
                    }
                }
            } else {
                kbd->key_count++;
                kbd->key_down = _tag;
                if (kbd->putc) {
                    int c = _tag;

                    if ((kbd->key_modifier & FLAG_CTRL) && (_tag >= '?') && (_tag <= '_')) c = (_tag - '@') & 0x7f;
                    kbd->putc(kbd->param, c);
                }
            }
        }
        if (evt & EVE_TOUCH_ETYPE_TAG_UP) {
            uint8_t _tag = t->tag_up;

            if (_tag >= TAG_SHIFT && _tag <= TAG_FN) {
                if (touch_idx == 0) {
                    uint8_t f = (1 << (_tag - TAG_SHIFT));

                    if (!((kbd->key_modifier_lock | kbd->key_modifier_sticky) & f)) {
                        kbd->key_modifier &= ~f;
                    }
                }
            } else {
                if (kbd->key_count) kbd->key_count--;
                if (!kbd->key_count) kbd->key_down = 0;
                if (kbd->key_modifier_sticky) {
                    if (touch_idx == 0) kbd->key_modifier &= ~kbd->key_modifier_sticky;
                    kbd->key_modifier_sticky = 0;
                }
            }
        }
        kbd->active = 1;
    } else {
        kbd->active = 0;
    }
    return kbd->active;
}

uint8_t eve_kbd_draw(EVEKbd *kbd) {
    if (kbd->active) {
        int x = kbd->x;
        int y = kbd->y;
        int w = kbd->w;
        int row_h = kbd->h / 5;
        int key_w = (w - 9 * KEY_SPACERX) / 10 + 1;
        int mod_w = key_w + key_w / 2;
        int key_h = row_h - KEY_SPACERY;


        eve_cmd_dl(SAVE_CONTEXT());
        eve_cmd(CMD_KEYS, "hhhhhhs", x, y + row_h * 0, w, key_h, KEY_FONT, kbd->key_down, kbd->key_modifier & (FLAG_FN | FLAG_SHIFT) ? "!@#$%^&*()" : (kbd->key_modifier & FLAG_CTRL ? " @[\\]^_?  " : "1234567890"));
        eve_cmd(CMD_KEYS, "hhhhhhs", x, y + row_h * 1, w, key_h, KEY_FONT, kbd->key_down, kbd->key_modifier & FLAG_FN ? "-_=+[]{}\\|" : kbd->key_modifier & (FLAG_SHIFT | FLAG_CTRL) ? "QWERTYUIOP" : "qwertyuiop");
        eve_cmd(CMD_KEYS, "hhhhhhs", x + key_w / 2, y + row_h * 2, w - key_w, key_h, KEY_FONT, kbd->key_down, kbd->key_modifier & FLAG_FN ? "`~   ;:'\"" : kbd->key_modifier & (FLAG_SHIFT | FLAG_CTRL) ? "ASDFGHJKL" : "asdfghjkl");
        eve_cmd(CMD_KEYS, "hhhhhhs", x + mod_w + KEY_SPACERX, y + row_h * 3, w - 2 * (mod_w + KEY_SPACERX), key_h, KEY_FONT, kbd->key_down, kbd->key_modifier & FLAG_FN ? " ,.<>/?" : kbd->key_modifier & (FLAG_SHIFT | FLAG_CTRL) ? "ZXCVBNM" : "zxcvbnm");
        eve_cmd_dl(TAG(TAG_SHIFT));
        eve_cmd(CMD_BUTTON, "hhhhhhs", x, y + row_h * 3, mod_w, key_h, MOD_FONT, kbd->key_modifier & FLAG_SHIFT ? EVE_OPT_FLAT : 0, "shift");
        eve_cmd_dl(TAG(KEY_BS));
        eve_cmd(CMD_BUTTON, "hhhhhhs", x + w - mod_w, y + row_h * 3, mod_w, key_h, MOD_FONT, kbd->key_down == KEY_BS ? EVE_OPT_FLAT : 0, "del");
        eve_cmd_dl(TAG(TAG_FN));
        eve_cmd(CMD_BUTTON, "hhhhhhs", x, y + row_h * 4, mod_w, key_h, MOD_FONT, kbd->key_modifier & FLAG_FN ? EVE_OPT_FLAT : 0, "fn");
        eve_cmd_dl(TAG(TAG_CTRL));
        eve_cmd(CMD_BUTTON, "hhhhhhs", x + mod_w + KEY_SPACERX, y + row_h * 4, mod_w, key_h, MOD_FONT, kbd->key_modifier & FLAG_CTRL ? EVE_OPT_FLAT : 0, "ctrl");
        eve_cmd_dl(TAG(' '));
        eve_cmd(CMD_BUTTON, "hhhhhhs", x + 2 * (mod_w + KEY_SPACERX), y + row_h * 4, w - 3 * (mod_w + KEY_SPACERX), key_h, MOD_FONT, kbd->key_down == ' ' ? EVE_OPT_FLAT : 0, "");
        eve_cmd_dl(TAG(KEY_RET));
        eve_cmd(CMD_BUTTON, "hhhhhhs", x + w - mod_w, y + row_h * 4, mod_w, key_h, MOD_FONT, kbd->key_down == KEY_RET ? EVE_OPT_FLAT : 0, "ret");
        eve_cmd_dl(RESTORE_CONTEXT());
    } else {
        eve_cmd(CMD_APPEND, "ww", kbd->mem_addr, kbd->mem_size);
    }
    return 0x7e;
}
