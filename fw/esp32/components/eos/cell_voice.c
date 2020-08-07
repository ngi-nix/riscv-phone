#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <esp_timer.h>
#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "at_cmd.h"
#include "cell.h"

static const char *TAG = "EOS VOICE";

static char cmd[256];
static char ring_buf[256];

void eos_cell_voice_handler(unsigned char mtype, unsigned char *buffer, uint16_t size) {
    int cmd_len, rv;

    rv = eos_modem_take(1000);
    if (rv) return;

    buffer += 1;
    size -= 1;
    switch (mtype) {
        case EOS_CELL_MTYPE_VOICE_DIAL:
            if (size == 0) return;

            buffer[size] = '\0';
            cmd_len = snprintf(cmd, sizeof(cmd), "ATD%s;\r", buffer);
            if ((cmd_len < 0) || (cmd_len >= sizeof(cmd))) return;
            at_cmd(cmd);
            rv = at_expect("^OK", "^ERROR", 1000);
            eos_cell_pcm_start();
            break;

        case EOS_CELL_MTYPE_VOICE_ANSWER:
            at_cmd("ATA\r");
            rv = at_expect("^OK", "^ERROR", 1000);
            eos_cell_pcm_start();
            break;

        case EOS_CELL_MTYPE_VOICE_HANGUP:
            eos_cell_pcm_stop();
            at_cmd("AT+CHUP\r");
            rv = at_expect("^OK", "^ERROR", 1000);
            break;
    }

    eos_modem_give();
}

static void ring_handler(char *urc, regmatch_t m[]) {
    unsigned char *buf;
    uint16_t len;
    uint32_t timeout = 1000, e = 0;
    uint64_t t_start = esp_timer_get_time();
    int rv = EOS_OK;

    ring_buf[0] = '\0';
    while (ring_buf[0] == '\0') {
        rv = eos_modem_readln(ring_buf, sizeof(ring_buf), timeout - e);
        if (rv) break;

        e = (uint32_t)(esp_timer_get_time() - t_start) / 1000;
        if (timeout && (e > timeout)) {
            rv = EOS_ERR_TIMEOUT;
            break;
        }
    }

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_RING;
    len = 1;
    if (!rv) {
        regex_t re;
        regmatch_t match[2];

        regcomp(&re, "^\\+CLIP: \"(\\+?[0-9]+)\"", REG_EXTENDED);
        if (regexec(&re, ring_buf, 2, match, 0) == 0) {
            ring_buf[match[1].rm_eo] = '\0';
            strcpy((char *)buf + 1, ring_buf + match[1].rm_so);
            len += 1 + match[1].rm_eo - match[1].rm_so;
        }
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
    int duration = 0;

    eos_cell_pcm_stop();

    sscanf(urc + m[1].rm_so, "%6d", &duration);
    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_VOICE | EOS_CELL_MTYPE_VOICE_MISSED;
    urc[m[1].rm_eo] = '\0';
    strcpy((char *)buf + 1, urc + m[1].rm_so);
    len = 2 + m[1].rm_eo - m[1].rm_so;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, len);
}

// MISSED_CALL: 02:18AM +381641733314
// +CLIP: "+381641733314",145,,,,0
// "+CLIP: \"(\\+?[0-9]+)\""

void eos_cell_voice_init(void) {
    at_urc_insert("^RING", ring_handler, REG_EXTENDED);
    at_urc_insert("^VOICE CALL: BEGIN", call_begin_handler, REG_EXTENDED);
    at_urc_insert("^VOICE CALL: END: ([0-9]{6}$)$", call_end_handler, REG_EXTENDED);
    at_urc_insert("^MISSED.CALL: [^ ]+ (\\+?[0-9]+)$", call_missed_handler, REG_EXTENDED);
}