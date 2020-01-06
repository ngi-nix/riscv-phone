#include "eve.h"
#include "eve_kbd.h"

#define FLAG_SHIFT  0x01
#define FLAG_CTRL   0x02
#define FLAG_FN     0x04

#define KEY_SHIFT   0x11
#define KEY_CTRL    0x12
#define KEY_FN      0x13

#define KEY_DEL     0x08
#define KEY_RET     0x0a

#define KEYS_Y      575
#define KEYS_FSIZE  29
#define KEYS_HEIGHT 40
#define KEYS_RSIZE  45

void eos_kbd_init(EOSKbd *kbd, eos_kbd_fptr_t key_down_f, uint32_t mem_addr, uint32_t *mem_next) {
    eos_eve_write16(REG_CMD_DL, 0);
    eos_kbd_update(kbd);
    eos_eve_cmd_exec(1);
    kbd->mem_addr = mem_addr;
    kbd->mem_size = eos_eve_read16(REG_CMD_DL);
    kbd->key_modifier = 0;
    kbd->key_count = 0;
    kbd->key_down = 0;
    kbd->key_down_f = key_down_f;
    eos_eve_cmd(CMD_MEMCPY, "www", kbd->mem_addr, EVE_RAM_DL, kbd->mem_size);
    eos_eve_cmd_exec(1);
    *mem_next = kbd->mem_addr + kbd->mem_size;
}

void eos_kbd_draw(EOSKbd *kbd, uint8_t tag0, int touch_idx) {
    uint8_t evt;
    EOSTouch *t = eos_touch_evt(tag0, touch_idx, 1, 127, &evt);

    if (t && evt) {
        if (evt & EOS_TOUCH_ETYPE_DOWN) {
            uint8_t _tag = t->tag;

            if (_tag >= KEY_SHIFT && _tag <= KEY_FN) {
                if (touch_idx == 0) {
                    uint8_t f = (1 << (_tag - KEY_SHIFT));

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
                if (kbd->key_down_f) {
                    int c = _tag;

                    if ((kbd->key_modifier & FLAG_CTRL) && (_tag >= '?') && (_tag <= '_')) c = (_tag - '@') & 0x7f;
                    kbd->key_down_f(c);
                }
            }
        }
        if (evt & EOS_TOUCH_ETYPE_UP) {
            uint8_t _tag = t->tag_prev;

            if (_tag >= KEY_SHIFT && _tag <= KEY_FN) {
                if (touch_idx == 0) {
                    uint8_t f = (1 << (_tag - KEY_SHIFT));

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
        eos_kbd_update(kbd);
    } else {
        eos_eve_cmd(CMD_APPEND, "ww", kbd->mem_addr, kbd->mem_size);
    }
}

void eos_kbd_update(EOSKbd *kbd) {
    eos_eve_cmd_dl(SAVE_CONTEXT());
    eos_eve_cmd(CMD_KEYS, "hhhhhhs", 0, KEYS_Y + KEYS_RSIZE * 0, 480, KEYS_HEIGHT, KEYS_FSIZE, kbd->key_down, kbd->key_modifier & (FLAG_FN | FLAG_SHIFT) ? "!@#$%^&*()" : (kbd->key_modifier & FLAG_CTRL ? " @[\\]^_?  " : "1234567890"));
    eos_eve_cmd(CMD_KEYS, "hhhhhhs", 0, KEYS_Y + KEYS_RSIZE * 1, 480, KEYS_HEIGHT, KEYS_FSIZE, kbd->key_down, kbd->key_modifier & FLAG_FN ? "-_=+[]{}\\|" : kbd->key_modifier & (FLAG_SHIFT | FLAG_CTRL) ? "QWERTYUIOP" : "qwertyuiop");
    eos_eve_cmd(CMD_KEYS, "hhhhhhs", 24, KEYS_Y + KEYS_RSIZE * 2, 432, KEYS_HEIGHT, KEYS_FSIZE, kbd->key_down, kbd->key_modifier & FLAG_FN ? "`~   ;:'\"" : kbd->key_modifier & (FLAG_SHIFT | FLAG_CTRL) ? "ASDFGHJKL" : "asdfghjkl");
    eos_eve_cmd(CMD_KEYS, "hhhhhhs", 72, KEYS_Y + KEYS_RSIZE * 3, 335, KEYS_HEIGHT, KEYS_FSIZE, kbd->key_down, kbd->key_modifier & FLAG_FN ? " ,.<>/?" : kbd->key_modifier & (FLAG_SHIFT | FLAG_CTRL) ? "ZXCVBNM" : "zxcvbnm");
    eos_eve_cmd_dl(TAG(KEY_SHIFT));
    eos_eve_cmd(CMD_BUTTON, "hhhhhhs", 0, KEYS_Y + KEYS_RSIZE * 3, 69, KEYS_HEIGHT, 21, kbd->key_modifier & FLAG_SHIFT ? EVE_OPT_FLAT : 0, "shift");
    eos_eve_cmd_dl(TAG(KEY_DEL));
    eos_eve_cmd(CMD_BUTTON, "hhhhhhs", 410, KEYS_Y + KEYS_RSIZE * 3, 70, KEYS_HEIGHT, 21, kbd->key_down == KEY_DEL ? EVE_OPT_FLAT : 0, "del");
    eos_eve_cmd_dl(TAG(KEY_FN));
    eos_eve_cmd(CMD_BUTTON, "hhhhhhs", 0, KEYS_Y + KEYS_RSIZE * 4, 69, KEYS_HEIGHT, 21, kbd->key_modifier & FLAG_FN ? EVE_OPT_FLAT : 0, "fn");
    eos_eve_cmd_dl(TAG(KEY_CTRL));
    eos_eve_cmd(CMD_BUTTON, "hhhhhhs", 72, KEYS_Y + KEYS_RSIZE * 4, 69, KEYS_HEIGHT, 21, kbd->key_modifier & FLAG_CTRL ? EVE_OPT_FLAT : 0, "ctrl");
    eos_eve_cmd_dl(TAG(' '));
    eos_eve_cmd(CMD_BUTTON, "hhhhhhs", 144, KEYS_Y + KEYS_RSIZE * 4, 263, KEYS_HEIGHT, 21, kbd->key_down == ' ' ? EVE_OPT_FLAT : 0, "");
    eos_eve_cmd_dl(TAG(KEY_RET));
    eos_eve_cmd(CMD_BUTTON, "hhhhhhs", 410, KEYS_Y + KEYS_RSIZE * 4, 69, KEYS_HEIGHT, 21, kbd->key_down == KEY_RET ? EVE_OPT_FLAT : 0, "ret");
    eos_eve_cmd_dl(RESTORE_CONTEXT());
}