#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <esp_timer.h>
#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "at_cmd.h"
#include "cell.h"

static char cmd[256];
static int cmd_len;

void eos_cell_voice_handler(unsigned char mtype, unsigned char *buffer, uint16_t buf_len) {
    int rv;

    switch (mtype) {
        case EOS_CELL_MTYPE_VOICE_DIAL:
            if (buf_len > EOS_CELL_MAX_DIAL_STR) return;

            buffer[buf_len] = '\0';
            cmd_len = snprintf(cmd, sizeof(cmd), "ATD%s;\r", buffer);
            if ((cmd_len < 0) || (cmd_len >= sizeof(cmd))) return;

            rv = eos_modem_take(1000);
            if (rv) return;

            at_cmd(cmd);
            rv = at_expect("^OK", "^ERROR", 1000);

            eos_modem_give();
            eos_cell_pcm_start();
            break;

        case EOS_CELL_MTYPE_VOICE_ANSWER:
            rv = eos_modem_take(1000);
            if (rv) return;

            at_cmd("ATA\r");
            rv = at_expect("^OK", "^ERROR", 1000);

            eos_modem_give();
            eos_cell_pcm_start();
            break;

        case EOS_CELL_MTYPE_VOICE_HANGUP:
            eos_cell_pcm_stop();

            rv = eos_modem_take(1000);
            if (rv) return;

            at_cmd("AT+CHUP\r");
            rv = at_expect("^OK", "^ERROR", 1000);

            eos_modem_give();
            break;

        case EOS_CELL_MTYPE_VOICE_PCM:
            eos_cell_pcm_push(buffer, buf_len);
            break;
    }
}

static void ring_handler(char *urc, regmatch_t m[]) {
    unsigned char *buf;
    char *ring_buf;
    uint16_t len;
    regmatch_t match[2];
    int rv = EOS_OK;

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_RING;
    len = 1;
    rv = at_expect_match("^\\+CLIP: \"(\\+?[0-9]+)\"", NULL, &ring_buf, match, 2, REG_EXTENDED, 1000);
    if (rv == 1) {
        ring_buf[match[1].rm_eo] = '\0';
        strcpy((char *)buf + 1, ring_buf + match[1].rm_so);
        len += 1 + match[1].rm_eo - match[1].rm_so;
    }
    eos_net_send(EOS_NET_MTYPE_CELL, buf, len);
}

static void call_begin_handler(char *urc, regmatch_t m[]) {
    unsigned char *buf;

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_BEGIN;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, 1);
}

static void call_end_handler(char *urc, regmatch_t m[]) {
    unsigned char *buf;
    int duration = 0;

    eos_cell_pcm_stop();

    sscanf(urc + m[1].rm_so, "%6d", &duration);
    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_END;
    buf[1] = duration >> 24;
    buf[2] = duration >> 16;
    buf[3] = duration >> 8;
    buf[4] = duration;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, 5);
}

static void call_missed_handler(char *urc, regmatch_t m[]) {
    unsigned char *buf;
    uint16_t len;

    eos_cell_pcm_stop();

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_MISS;
    urc[m[1].rm_eo] = '\0';
    strcpy((char *)buf + 1, urc + m[1].rm_so);
    len = 2 + m[1].rm_eo - m[1].rm_so;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, len);
}

void eos_cell_voice_init(void) {
    at_urc_insert("^RING", ring_handler, REG_EXTENDED);
    at_urc_insert("^VOICE CALL: BEGIN", call_begin_handler, REG_EXTENDED);
    at_urc_insert("^VOICE CALL: END: ([0-9]{6}$)$", call_end_handler, REG_EXTENDED);
    at_urc_insert("^MISSED.CALL: [^ ]+ (\\+?[0-9]+)$", call_missed_handler, REG_EXTENDED);
}