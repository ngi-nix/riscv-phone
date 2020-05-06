#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "eos.h"
#include "at_urc.h"

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
    int r = EOS_OK;

    if (strlen(pattern) >= AT_SIZE_PATTERN) return EOS_ERR;

    r = regcomp(&urc_list.item[urc_list.len].re, pattern, flags);
    if (r) return EOS_ERR;

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (urc_list.len == AT_SIZE_URC_LIST) r = EOS_ERR_FULL;

    if (!r) {
        strcpy(urc_list.item[urc_list.len].pattern, pattern);
        urc_list.item[urc_list.len].cb = cb;
        urc_list.len++;
    }

    xSemaphoreGive(mutex);

    return r;
}

int at_urc_delete(char *pattern) {
    int i;
    int r = EOS_ERR_NOTFOUND;

    xSemaphoreTake(mutex, portMAX_DELAY);

    for (i=0; i<urc_list.len; i++) {
        if ((strcmp(pattern, urc_list.item[i].pattern) == 0)) {
            if (i != urc_list.len - 1) memmove(&urc_list.item[i], &urc_list.item[i + 1], (urc_list.len - i - 1) * sizeof(ATURCItem));
            urc_list.len--;
            if (urc_curr) {
                if (urc_curr == &urc_list.item[i]) {
                    urc_curr = NULL;
                } else if (urc_curr > &urc_list.item[i]) {
                    urc_curr--;
                }
            }

            r = EOS_OK;
            break;
        }
    }

    xSemaphoreGive(mutex);

    return r;
}
