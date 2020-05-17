#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>

#include "eos.h"

#include "cell.h"
#include "at_cmd.h"

static const char *TAG = "EOS ATCMD";

typedef struct ATURCItem {
    regex_t re;
    at_urc_cb_t cb;
    char pattern[AT_SIZE_PATTERN];
} ATURCItem;

typedef struct ATURCList {
    ATURCItem item[AT_SIZE_URC_LIST];
    int len;
} ATURCList;

static ATURCList urc_list;
static ATURCItem *urc_curr;
static SemaphoreHandle_t mutex;

static char at_buf[128];

void at_init(void) {
    memset(&urc_list, 0, sizeof(ATURCList));

    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);
}

int at_urc_process(char *urc) {
    regmatch_t match[AT_SIZE_NMATCH];
    at_urc_cb_t cb = NULL;
    regmatch_t *m = NULL;

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (urc_curr == NULL) {
        int i;

        for (i=0; i<urc_list.len; i++) {
            if (regexec(&urc_list.item[i].re, urc, AT_SIZE_NMATCH, match, 0) == 0) {
                urc_curr = &urc_list.item[i];
                m = match;
                break;
            }
        }
    }
    if (urc_curr) cb = urc_curr->cb;

    xSemaphoreGive(mutex);

    if (cb) {
        int r = cb(urc, m);

        if (r != AT_URC_MORE) {
            xSemaphoreTake(mutex, portMAX_DELAY);
            urc_curr = NULL;
            xSemaphoreGive(mutex);
        }
        return 1;
    }
    return 0;
}

int at_urc_insert(char *pattern, at_urc_cb_t cb, int flags) {
    int r;
    int rv = EOS_OK;

    if (strlen(pattern) >= AT_SIZE_PATTERN) return EOS_ERR;

    xSemaphoreTake(mutex, portMAX_DELAY);

    r = regcomp(&urc_list.item[urc_list.len].re, pattern, flags);
    if (r) rv = EOS_ERR;

    if (!rv && (urc_list.len == AT_SIZE_URC_LIST)) rv = EOS_ERR_FULL;
    if (!rv) {
        strcpy(urc_list.item[urc_list.len].pattern, pattern);
        urc_list.item[urc_list.len].cb = cb;
        urc_list.len++;
    }

    xSemaphoreGive(mutex);

    return rv;
}

int at_urc_delete(char *pattern) {
    int i;
    int rv = EOS_ERR_NOTFOUND;

    xSemaphoreTake(mutex, portMAX_DELAY);

    for (i=0; i<urc_list.len; i++) {
        if ((strcmp(pattern, urc_list.item[i].pattern) == 0)) {
            if (i != urc_list.len - 1) memmove(&urc_list.item[i], &urc_list.item[i + 1], (urc_list.len - i - 1) * sizeof(ATURCItem));
            urc_list.len--;
            memset(&urc_list.item[urc_list.len], 0, sizeof(ATURCItem));
            if (urc_curr) {
                if (urc_curr == &urc_list.item[i]) {
                    urc_curr = NULL;
                } else if (urc_curr > &urc_list.item[i]) {
                    urc_curr--;
                }
            }
            rv = EOS_OK;
            break;
        }
    }

    xSemaphoreGive(mutex);

    return rv;
}

int at_expect(char *str_ok, char *str_err, uint32_t timeout) {
    int r;
    int rv;
    regex_t re_ok;
    regex_t re_err;
    uint32_t e = 0;
    uint64_t t_start = esp_timer_get_time();

    if (str_ok) {
        rv = regcomp(&re_ok, str_ok, REG_EXTENDED | REG_NOSUB);
        if (rv) return EOS_ERR;
    }

    if (str_err) {
        rv = regcomp(&re_err, str_err, REG_EXTENDED | REG_NOSUB);
        if (rv) return EOS_ERR;
    }

    do {
        rv = eos_modem_readln(at_buf, sizeof(at_buf), timeout - e);
        if (rv) return rv;

        if (at_buf[0] == '\0') continue;

        if (str_ok && (regexec(&re_ok, at_buf, 0, NULL, 0) == 0)) return 1;
        if (str_err && (regexec(&re_err, at_buf, 0, NULL, 0) == 0)) return 0;

        r = at_urc_process(at_buf);
        if (!r) ESP_LOGD(TAG, "expect unhandled URC: %s", at_buf);

        e = (uint32_t)(esp_timer_get_time() - t_start) / 1000;
        if (e >= timeout) return EOS_ERR_TIMEOUT;
    } while (1);
}
