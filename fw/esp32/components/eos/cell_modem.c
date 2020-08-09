#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <netif/ppp/pppos.h>
#include <netif/ppp/pppapi.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "power.h"

#include "at_cmd.h"
#include "cell.h"

// XXX: PPP reconnect on failure

#define UART_SIZE_IO_BUF    8192

#define UART_GPIO_TXD       16
#define UART_GPIO_RXD       17
#define UART_GPIO_DTR       32
#define UART_GPIO_RI        35

#define MODEM_ETYPE_INIT    1
#define MODEM_ETYPE_RI      2

#define AT_CMD_INIT_SIZE    4

#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

static const char *TAG = "EOS MODEM";

static char *at_cmd_init[AT_CMD_INIT_SIZE] = {
    "AT+CFGRI=1\r",
    "AT+CSCLK=1\r",
    "AT+CLIP=1\r",
    "AT+CMGF=0\r"
};

static SemaphoreHandle_t mutex;

static QueueHandle_t modem_queue;
static QueueHandle_t uart_queue;

static char urc_buf[EOS_CELL_UART_SIZE_BUF];
static char uart_buf[EOS_CELL_UART_SIZE_BUF];
static size_t uart_buf_len;
static char uart_buf_dirty = 0;

static uint8_t uart_mode = EOS_CELL_UART_MODE_ATCMD;
static uint8_t _uart_mode = EOS_CELL_UART_MODE_UNDEF;
static SemaphoreHandle_t uart_mutex;

static char ppp_apn[64];
static char ppp_user[64];
static char ppp_pass[64];
static SemaphoreHandle_t ppp_mutex;

static ppp_pcb *ppp_handle;
static struct netif ppp_netif;

typedef enum {
    UART_EEVT_MODE = UART_EVENT_MAX
} uart_eevt_type_t;

typedef struct {
    uint8_t type;
} modem_event_t;

static void modem_atcmd_read(size_t bsize);

static void uart_data_read(uint8_t mode) {
    unsigned char *buf;
    int rd;
    size_t bsize;

    uart_get_buffered_data_len(UART_NUM_2, &bsize);
    switch (mode) {
        case EOS_CELL_UART_MODE_ATCMD:
            modem_atcmd_read(bsize);
            break;

        case EOS_CELL_UART_MODE_PPP:
            rd = 0;

            do {
                int _rd = eos_modem_read(uart_buf, MIN(bsize - rd, sizeof(uart_buf)), 100);
                if (ppp_handle) pppos_input_tcpip(ppp_handle, (uint8_t *)uart_buf, _rd);
                rd += _rd;
            } while (rd != bsize);
            break;

        case EOS_CELL_UART_MODE_RELAY:
            rd = 0;

            do {
                int _rd;

                buf = eos_net_alloc();
                buf[0] = EOS_CELL_MTYPE_UART_DATA;
                _rd = eos_modem_read(buf + 1, MIN(bsize - rd, EOS_NET_SIZE_BUF - 1), 100);
                eos_net_send(EOS_NET_MTYPE_CELL, buf, _rd + 1);
                rd += _rd;
            } while (rd != bsize);
            break;

        default:
            break;

    }
}

static void uart_event_task(void *pvParameters) {
    char mode = EOS_CELL_UART_MODE_ATCMD;
    char _mode = EOS_CELL_UART_MODE_ATCMD;
    uart_event_t event;

    xSemaphoreTake(uart_mutex, portMAX_DELAY);
    while (1) {
        /* Waiting for UART event.
         */
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    /* Event of UART receiving data
                     */
                    if (mode != EOS_CELL_UART_MODE_NONE) uart_data_read(mode);
                    if ((mode != _mode) && (uart_buf_len == 0)) {
                        if (_mode == EOS_CELL_UART_MODE_NONE) xSemaphoreGive(uart_mutex);
                        mode = _mode;
                    }
                    break;

                case UART_EEVT_MODE:
                    /* Mode change
                     */
                    _mode = (char)event.size;
                    if ((_mode != mode) && ((uart_buf_len == 0) || (mode == EOS_CELL_UART_MODE_NONE))) {
                        if (mode == EOS_CELL_UART_MODE_NONE) {
                            xSemaphoreTake(uart_mutex, portMAX_DELAY);
                            uart_data_read(_mode);
                        }
                        if (_mode == EOS_CELL_UART_MODE_NONE) xSemaphoreGive(uart_mutex);
                        mode = _mode;
                    }
                    break;

                default:
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

static void IRAM_ATTR uart_ri_isr_handler(void *arg) {
    modem_event_t evt;

    evt.type = MODEM_ETYPE_RI;
    xQueueSendFromISR(modem_queue, &evt, NULL);
}

static int modem_atcmd_init(void) {
    unsigned char *buf;
    int echo_on = 0;
    int tries = 3;
    int i, r;
    int rv = EOS_OK;

    rv = eos_modem_take(1000);
    if (rv) return rv;

    do {
        at_cmd("AT\r");
        r = at_expect("^AT", "^OK", 1000);
        if (r >= 0) {
            echo_on = r;
            if (echo_on) {
                r = at_expect("^OK", NULL, 1000);
            }
            break;
        }
        tries--;
    } while (tries);

    if (tries == 0) {
        eos_modem_give();
        return EOS_ERR_TIMEOUT;
    }

    if (echo_on) {
        at_cmd("AT&F\r");
        r = at_expect("^AT&F", NULL, 1000);
        r = at_expect("^OK", NULL, 1000);
    } else {
        at_cmd("AT&F\r");
        r = at_expect("^OK", NULL, 1000);

    }
    at_cmd("ATE0\r");
    r = at_expect("^ATE0", NULL, 1000);
    r = at_expect("^OK", "^ERROR", 1000);

    for (i=0; i<AT_CMD_INIT_SIZE; i++) {
        at_cmd(at_cmd_init[i]);
        r = at_expect("^OK", "^ERROR", 1000);
    }

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_READY;
    eos_net_send(EOS_NET_MTYPE_CELL, buf, 1);

    eos_modem_give();

    return EOS_OK;
}

static void modem_atcmd_read(size_t bsize) {
    char *ln_end;
    int rd = 0;

    do {
        char *uart_curr = uart_buf + uart_buf_len;
        int _rd = eos_modem_read(uart_curr, MIN(bsize - rd, sizeof(uart_buf) - uart_buf_len - 1), 100);

        rd += _rd;
        uart_buf_len += _rd;
        uart_buf[uart_buf_len] = '\0';
        while ((ln_end = strchr(uart_curr, '\n'))) {
            char *_ln_end = ln_end;

            while ((_ln_end > uart_buf) && (*(_ln_end - 1) == '\r')) _ln_end--;
            memcpy(urc_buf, uart_buf, _ln_end - uart_buf);
            urc_buf[_ln_end - uart_buf] = '\0';

            uart_buf_len -= ln_end - uart_buf + 1;
            if (uart_buf_len) memmove(uart_buf, ln_end + 1, uart_buf_len);
            if (!uart_buf_dirty) at_urc_process(urc_buf);

            uart_curr = uart_buf;
            uart_buf[uart_buf_len] = '\0';
            uart_buf_dirty = 0;
        }
        if (uart_buf_len == sizeof(uart_buf) - 1) {
            uart_buf_len = 0;
            uart_buf_dirty = 1;
        }
    } while (rd != bsize);
}

static void modem_urc_init_handler(char *urc, regmatch_t m[]) {
    modem_event_t evt;

    evt.type = MODEM_ETYPE_INIT;
    xQueueSend(modem_queue, &evt, portMAX_DELAY);
}

static void modem_set_mode(uint8_t mode) {
    uart_event_t evt;

    evt.type = UART_EEVT_MODE;
    evt.size = mode;
    xQueueSend(uart_queue, &evt, portMAX_DELAY);
}

static void modem_event_task(void *pvParameters) {
    modem_event_t evt;

    while (1) {
        if (xQueueReceive(modem_queue, &evt, portMAX_DELAY)) {
            switch (evt.type) {
                case MODEM_ETYPE_INIT:
                    modem_atcmd_init();
                    break;

                case MODEM_ETYPE_RI:
                    ESP_LOGI(TAG, "URC from RI");
                    break;

                default:
                    break;
            }

            /* Obsolete!!!
            uint64_t t_start = esp_timer_get_time();
            if (xQueueReceive(modem_queue, &level, 200 / portTICK_RATE_MS) && (level == 1)) {
                uint64_t t_end = esp_timer_get_time();
                ESP_LOGI(TAG, "URC:%u", (uint32_t)(t_end - t_start));
            } else {
                ESP_LOGI(TAG, "RING");
            }
            */

        }
    }
    vTaskDelete(NULL);
}

static char *memstr(char *mem, size_t size, char *str) {
    size_t i = 0;
    char *max_mem;

    if (str[0] == '\0') return NULL;

    max_mem = mem + size;

    while (mem < max_mem) {
        if (*mem != str[i]) {
            mem -= i;
            i = 0;
        } else {
            if (str[i+1] == '\0') return mem - i;
            i++;
        }
        mem++;
    }

    return NULL;
}

static uint32_t ppp_output_cb(ppp_pcb *pcb, uint8_t *data, uint32_t len, void *ctx) {
    size_t rv;

	xSemaphoreTake(ppp_mutex, portMAX_DELAY);
    rv = eos_modem_write(data, len);
	xSemaphoreGive(ppp_mutex);

    return rv;
}

/* PPP status callback */
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
    struct netif *pppif = ppp_netif(pcb);
    LWIP_UNUSED_ARG(ctx);

    switch(err_code) {
        case PPPERR_NONE: {
            ESP_LOGI(TAG, "status_cb: Connect");
            ESP_LOGI(TAG,"   our_ipaddr  = %s\n", ipaddr_ntoa(&pppif->ip_addr));
            ESP_LOGI(TAG,"   his_ipaddr  = %s\n", ipaddr_ntoa(&pppif->gw));
            ESP_LOGI(TAG,"   netmask     = %s\n", ipaddr_ntoa(&pppif->netmask));
            return;
        }
        case PPPERR_PARAM: {
            ESP_LOGE(TAG, "status_cb: Invalid parameter");
            break;
        }
        case PPPERR_OPEN: {
            ESP_LOGE(TAG, "status_cb: Unable to open PPP session");
            break;
        }
        case PPPERR_DEVICE: {
            ESP_LOGE(TAG, "status_cb: Invalid I/O device for PPP");
            break;
        }
        case PPPERR_ALLOC: {
            ESP_LOGE(TAG, "status_cb: Unable to allocate resources");
            break;
        }
        case PPPERR_USER: {
            ESP_LOGI(TAG, "status_cb: Disconnect");
            break;
        }
        case PPPERR_CONNECT: {
            ESP_LOGE(TAG, "status_cb: Connection lost");
            break;
        }
        case PPPERR_AUTHFAIL: {
            ESP_LOGE(TAG, "status_cb: Failed authentication challenge");
            break;
        }
        case PPPERR_PROTOCOL: {
            ESP_LOGE(TAG, "status_cb: Failed to meet protocol");
            break;
        }
        case PPPERR_PEERDEAD: {
            ESP_LOGE(TAG, "status_cb: Connection timeout");
            break;
        }
        case PPPERR_IDLETIMEOUT: {
            ESP_LOGE(TAG, "status_cb: Idle Timeout");
            break;
        }
        case PPPERR_CONNECTTIME: {
            ESP_LOGE(TAG, "status_cb: Max connect time reached");
            break;
        }
        case PPPERR_LOOPBACK: {
            ESP_LOGE(TAG, "status_cb: Loopback detected");
            break;
        }
        default: {
            ESP_LOGE(TAG, "status_cb: Unknown error code %d", err_code);
            break;
        }
    }

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (_uart_mode == EOS_CELL_UART_MODE_UNDEF) _uart_mode = EOS_CELL_UART_MODE_ATCMD;
    uart_mode = _uart_mode;
    _uart_mode = EOS_CELL_UART_MODE_UNDEF;

    modem_set_mode(EOS_CELL_UART_MODE_NONE);
    xSemaphoreTake(uart_mutex, portMAX_DELAY);

    ppp_handle = NULL;
    modem_set_mode(uart_mode);

    xSemaphoreGive(uart_mutex);
    xSemaphoreGive(mutex);

    ppp_free(pcb);
}

static int ppp_pause(uint32_t timeout) {
    int done = 0;
    int len = 0;
    int rv = EOS_OK;
    int start = 0;
    int r;

    char *ok_str = NULL;
    uint64_t t_start;
    uint32_t dt, _dt;

    modem_set_mode(EOS_CELL_UART_MODE_NONE);

    t_start = esp_timer_get_time();
    r = xSemaphoreTake(uart_mutex, timeout ? timeout / portTICK_PERIOD_MS : portMAX_DELAY);
    if (r == pdFALSE) return EOS_ERR_TIMEOUT;
    if (timeout) {
        dt = ((esp_timer_get_time() - t_start) / 1000);
        if (dt >= timeout) {
            modem_set_mode(EOS_CELL_UART_MODE_PPP);
            xSemaphoreGive(uart_mutex);
            return EOS_ERR_TIMEOUT;
        }
    }
    r = xSemaphoreTake(ppp_mutex, timeout ? (timeout - dt) / portTICK_PERIOD_MS : portMAX_DELAY);
    if (r == pdFALSE) {
        modem_set_mode(EOS_CELL_UART_MODE_PPP);
        xSemaphoreGive(uart_mutex);
        return EOS_ERR_TIMEOUT;
    }
    eos_modem_flush();

    _dt = ((esp_timer_get_time() - t_start) / 1000);
    do {
        len = eos_modem_read(uart_buf + uart_buf_len, sizeof(uart_buf) - uart_buf_len, 10);
        dt = ((esp_timer_get_time() - t_start) / 1000);
        if ((dt - _dt) >= 1000) {
            _dt =dt;
            at_cmd("+++");
            start = 1;
        }
        if (start && (len > 0)) {
            if (uart_buf_len > 5) {
                ok_str = memstr(uart_buf + uart_buf_len - 5, len + 5, "\r\nOK\r\n");
            } else {
                ok_str = memstr(uart_buf, uart_buf_len + len, "\r\nOK\r\n");
            }
            uart_buf_len += len;
        }
        if (ok_str) {
            if (ppp_handle) pppos_input_tcpip(ppp_handle, (uint8_t *)uart_buf, ok_str - uart_buf);
            ok_str += 6;
            uart_buf_len -= ok_str - uart_buf;
            if (uart_buf_len) memmove(uart_buf, ok_str, uart_buf_len);
            done = 1;
        } else if (uart_buf_len == sizeof(uart_buf)) {
            if (ppp_handle) pppos_input_tcpip(ppp_handle, (uint8_t *)uart_buf, sizeof(uart_buf) / 2);
            memcpy(uart_buf, uart_buf + sizeof(uart_buf) / 2, sizeof(uart_buf) / 2);
            uart_buf_len = sizeof(uart_buf) / 2;
        }
        if (!done && timeout && (dt >= timeout)) {
            modem_set_mode(EOS_CELL_UART_MODE_PPP);
            xSemaphoreGive(uart_mutex);
            xSemaphoreGive(ppp_mutex);
            return EOS_ERR_TIMEOUT;
        }
    } while (!done);

    return rv;
}

static int ppp_resume(void) {
    int r;
    int rv = EOS_OK;

    at_cmd("ATO\r");
    r = at_expect("^CONNECT", "^(ERROR|NO CARRIER)", 1000);
    if (r <= 0) rv = EOS_ERR;

    modem_set_mode(EOS_CELL_UART_MODE_PPP);
    xSemaphoreGive(uart_mutex);
    xSemaphoreGive(ppp_mutex);

    return rv;
}

static int ppp_setup(void) {
    int r;
    char cmd[64];
    int cmd_len = snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"\r", ppp_apn);

    if ((cmd_len < 0) || (cmd_len >= sizeof(cmd))) return EOS_ERR;

    modem_set_mode(EOS_CELL_UART_MODE_NONE);
    r = xSemaphoreTake(uart_mutex, 1000 / portTICK_PERIOD_MS);
    if (r == pdFALSE) {
        modem_set_mode(uart_mode);
        return EOS_ERR_TIMEOUT;
    }

    at_cmd(cmd);
    r = at_expect("^OK", "^ERROR", 1000);
    if (r <= 0) {
        modem_set_mode(uart_mode);
        xSemaphoreGive(uart_mutex);
        return EOS_ERR;
    }

    at_cmd("AT+CGDATA=\"PPP\",1\r");
    r = at_expect("^CONNECT", "^NO CARRIER", 1000);
    if (r <= 0) {
        modem_set_mode(uart_mode);
        xSemaphoreGive(uart_mutex);
        return EOS_ERR;
    }

    ppp_handle = pppapi_pppos_create(&ppp_netif, ppp_output_cb, ppp_status_cb, NULL);
    ppp_set_usepeerdns(ppp_handle, 1);
    ppp_set_default(ppp_handle);
    ppp_set_auth(ppp_handle, PPPAUTHTYPE_ANY, ppp_user, ppp_pass);
    ppp_connect(ppp_handle, 0);

    modem_set_mode(EOS_CELL_UART_MODE_PPP);
    xSemaphoreGive(uart_mutex);

    return EOS_OK;
}

void eos_modem_init(void) {
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
       .baud_rate = 115200,
       .data_bits = UART_DATA_8_BITS,
       .parity    = UART_PARITY_DISABLE,
       .stop_bits = UART_STOP_BITS_1,
       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, UART_GPIO_TXD, UART_GPIO_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, UART_SIZE_IO_BUF, UART_SIZE_IO_BUF, 10, &uart_queue, 0);

    // Configuration for the DTR/RI lines
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << UART_GPIO_DTR);
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(UART_GPIO_DTR, 0);

    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << UART_GPIO_RI);
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);

    uart_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(uart_mutex);

    ppp_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(ppp_mutex);

    modem_queue = xQueueCreate(4, sizeof(modem_event_t));
    xTaskCreate(uart_event_task, "uart_event", EOS_TASK_SSIZE_UART, NULL, EOS_TASK_PRIORITY_UART, NULL);
    xTaskCreate(modem_event_task, "modem_event", EOS_TASK_SSIZE_MODEM, NULL, EOS_TASK_PRIORITY_MODEM, NULL);

    gpio_isr_handler_add(UART_GPIO_RI, uart_ri_isr_handler, NULL);

    at_init();
    at_urc_insert("^PB DONE", modem_urc_init_handler, REG_EXTENDED);
    eos_modem_set_mode(EOS_CELL_UART_MODE_ATCMD);

    eos_cell_voice_init();
    eos_cell_sms_init();
    eos_cell_ussd_init();
    ESP_LOGI(TAG, "INIT");
}

void eos_modem_flush(void){
    uart_wait_tx_done(UART_NUM_2, portMAX_DELAY);
}

size_t eos_modem_write(void *data, size_t size) {
    return uart_write_bytes(UART_NUM_2, (const char *)data, size);
}

size_t eos_modem_read(void *data, size_t size, uint32_t timeout) {
    return uart_read_bytes(UART_NUM_2, (uint8_t *)data, size, timeout / portTICK_RATE_MS);
}

int eos_modem_readln(char *buf, size_t buf_size, uint32_t timeout) {
    char *ln_end = NULL;
    size_t buf_len = 0;
    uint64_t t_start = esp_timer_get_time();

    buf[0] = '\0';
    if (uart_buf_len) {
        buf_len = MIN(buf_size -1, uart_buf_len);
        memcpy(buf, uart_buf, buf_len);
        buf[buf_len] = '\0';
        ln_end = strchr(buf, '\n');

        uart_buf_len -= buf_len;
        if (uart_buf_len) memmove(uart_buf, uart_buf + buf_len, uart_buf_len);
    }

    while (ln_end == NULL) {
        int rv = EOS_OK;
        int len;

        if (buf_len == buf_size - 1) rv = EOS_ERR_FULL;
        if (!rv && timeout && ((uint32_t)((esp_timer_get_time() - t_start) / 1000) > timeout)) rv = EOS_ERR_TIMEOUT;
        if (rv) {
            uart_buf_dirty = 1;
            return rv;
        }

        len = eos_modem_read(buf + buf_len, MIN(buf_size - buf_len - 1, sizeof(uart_buf) - uart_buf_len), 10);
        if (len > 0) {
            buf[buf_len + len] = '\0';
            ln_end = strchr(buf + buf_len, '\n');
            buf_len += len;
        }
    }
    buf_len -= ln_end - buf + 1;
    if (buf_len) {
        if (uart_buf_len) memmove(uart_buf + buf_len, uart_buf, uart_buf_len);
        memcpy(uart_buf, ln_end + 1, buf_len);
        uart_buf_len += buf_len;
    }

    while ((ln_end > buf) && (*(ln_end - 1) == '\r')) ln_end--;
    *ln_end = '\0';

    return EOS_OK;
}

uint8_t eos_modem_get_mode(void) {
    uint8_t ret;

    xSemaphoreTake(mutex, portMAX_DELAY);
    ret = uart_mode;
    xSemaphoreGive(mutex);

    return ret;
}

int eos_modem_set_mode(uint8_t mode) {
    int rv = EOS_OK;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (mode != uart_mode) {
        if ((uart_mode == EOS_CELL_UART_MODE_PPP) && ppp_handle) {
            _uart_mode = mode;
            pppapi_close(ppp_handle, 0);
        } else {
            if (mode == EOS_CELL_UART_MODE_PPP) {
                rv = ppp_setup();
            } else {
                modem_set_mode(mode);
            }
            if (!rv) uart_mode = mode;
        }
    }
    xSemaphoreGive(mutex);

    return rv;
}

int eos_modem_take(uint32_t timeout) {
    int rv = EOS_OK;

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (uart_mode == EOS_CELL_UART_MODE_PPP) {
        rv = ppp_pause(timeout);
    } else {
        int r;

        modem_set_mode(EOS_CELL_UART_MODE_NONE);
        r = xSemaphoreTake(uart_mutex, timeout ? timeout / portTICK_PERIOD_MS : portMAX_DELAY);
        if (r == pdFALSE) {
            modem_set_mode(uart_mode);
            rv = EOS_ERR_TIMEOUT;
        }
    }

    if (rv) xSemaphoreGive(mutex);

    return rv;
}

void eos_modem_give(void) {
    if (uart_mode == EOS_CELL_UART_MODE_PPP) {
        int rv = ppp_resume();
        if (rv) ESP_LOGW(TAG, "PPP resume failed");
    } else {
        modem_set_mode(uart_mode);
        xSemaphoreGive(uart_mutex);
    }
    xSemaphoreGive(mutex);
}

void eos_modem_sleep(uint8_t mode) {
    int r;

    xSemaphoreTake(mutex, portMAX_DELAY);
    modem_set_mode(EOS_CELL_UART_MODE_NONE);
    r = xSemaphoreTake(uart_mutex, 1000 / portTICK_PERIOD_MS);
    if (r == pdFALSE) {
        ESP_LOGE(TAG, "Obtaining mutex before sleep failed");
    }
    gpio_set_level(UART_GPIO_DTR, 1);
    if (mode == EOS_PWR_SMODE_DEEP) {
        gpio_hold_en(UART_GPIO_DTR);
    }
}

void eos_modem_wake(uint8_t source, uint8_t mode) {
    if (source == EOS_PWR_WAKE_UART) {
        modem_event_t evt;

        evt.type = MODEM_ETYPE_RI;
        xQueueSend(modem_queue, &evt, portMAX_DELAY);
    }

    if (mode != EOS_PWR_SMODE_DEEP) {
        gpio_set_intr_type(UART_GPIO_RI, GPIO_INTR_NEGEDGE);
        gpio_isr_handler_add(UART_GPIO_RI, uart_ri_isr_handler, NULL);
        gpio_set_level(UART_GPIO_DTR, 0);

        modem_set_mode(uart_mode);
        xSemaphoreGive(uart_mutex);
        xSemaphoreGive(mutex);
    } else {
        gpio_hold_dis(UART_GPIO_DTR);
    }
}

void eos_ppp_set_apn(char *apn) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    strncpy(ppp_apn, apn, sizeof(ppp_apn) - 1);
    xSemaphoreGive(mutex);
}

void eos_ppp_set_auth(char *user, char *pass) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    strncpy(ppp_user, user, sizeof(ppp_user) - 1);
    strncpy(ppp_pass, pass, sizeof(ppp_pass) - 1);
    xSemaphoreGive(mutex);
}

int eos_ppp_connect(void) {
    return eos_modem_set_mode(EOS_CELL_UART_MODE_PPP);
}

void eos_ppp_disconnect(void) {
    eos_modem_set_mode(EOS_CELL_UART_MODE_ATCMD);
}