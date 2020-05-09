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
#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "at_urc.h"
#include "cell.h"

// XXX: Modem init, reconnect on failure

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

#define UART_SIZE_BUF       1024
#define UART_SIZE_URC_BUF   128

#define UART_GPIO_TXD       16
#define UART_GPIO_RXD       17
#define UART_GPIO_DTR       32
#define UART_GPIO_RI        35

static QueueHandle_t uart_queue;
static QueueHandle_t uart_ri_queue;

static const char *TAG = "EOS MODEM";

static SemaphoreHandle_t mutex;

static char at_resp[128];
static char uart_buf[UART_SIZE_URC_BUF];
static unsigned int uart_buf_len;
static uint8_t uart_mode = EOS_CELL_UART_MODE_NONE;
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
                pppos_input_tcpip(ppp_handle, (uint8_t *)uart_buf, _rd);
                rd += _rd;
            } while (rd != bsize);
            break;

        case EOS_CELL_UART_MODE_RELAY:
            rd = 0;

            do {
                int _rd;

                buf = eos_net_alloc();
                buf[0] = EOS_CELL_MTYPE_DATA;
                _rd = eos_modem_read(buf + 1, MIN(bsize - rd, EOS_NET_SIZE_BUF - 1), 100);
                eos_net_send(EOS_NET_MTYPE_CELL, buf, _rd + 1, 0);
                rd += _rd;
            } while (rd != bsize);
            break;

        default:
            break;

    }
}

static void uart_event_task(void *pvParameters) {
    char mode = EOS_CELL_UART_MODE_NONE;
    char _mode = EOS_CELL_UART_MODE_NONE;
    uart_event_t event;

    while (1) {
        /* Waiting for UART event.
         */
        if (xQueueReceive(uart_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
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

static void uart_ri_event_task(void *pvParameters) {
    int level;

    while (1) {
        if (xQueueReceive(uart_ri_queue, (void *)&level, (portTickType)portMAX_DELAY) && (level == 0)) {
            uint64_t t_start = esp_timer_get_time();
            if (xQueueReceive(uart_ri_queue, (void *)&level, 200 / portTICK_RATE_MS) && (level == 1)) {
                uint64_t t_end = esp_timer_get_time();
                ESP_LOGI(TAG, "URC:%u", (uint32_t)(t_end - t_start));
            } else {
                ESP_LOGI(TAG, "RING");
            }
        }
    }
    vTaskDelete(NULL);
}

static void IRAM_ATTR uart_ri_isr_handler(void *arg) {
    int level = gpio_get_level(UART_GPIO_RI);
    xQueueSendFromISR(uart_ri_queue, &level, NULL);
}

static void modem_atcmd_read(size_t bsize) {
    char *ln_begin, *ln_end, *ln_next;
    int rd = 0;

    do {
        int _rd = eos_modem_read(uart_buf + uart_buf_len, MIN(bsize - rd, sizeof(uart_buf) - uart_buf_len - 1), 100);
        ln_next = uart_buf + uart_buf_len;
        ln_begin = uart_buf;
        uart_buf_len += _rd;
        rd += _rd;
        uart_buf[uart_buf_len] = '\0';
        while ((ln_end = strchr(ln_next, '\n'))) {
            ln_end--;
            if ((ln_end > ln_begin) && (*ln_end == '\r')) {
                *ln_end = '\0';
                at_urc_process(ln_begin);
            }
            ln_next = ln_end + 2;
            ln_begin = ln_next;
        }
        if (ln_begin != uart_buf) {
            uart_buf_len -= ln_begin - uart_buf;
            if (uart_buf_len) memmove(uart_buf, ln_begin, uart_buf_len);
        } else if (uart_buf_len == sizeof(uart_buf) - 1) {
            memcpy(uart_buf, uart_buf + sizeof(uart_buf) / 2, sizeof(uart_buf) / 2);
            uart_buf_len = sizeof(uart_buf) / 2;
        }
    } while (rd != bsize);
}

static void modem_set_mode(uint8_t mode) {
    uart_event_t evt;

    evt.type = UART_EEVT_MODE;
    evt.size = mode;
    xQueueSend(uart_queue, (void *)&evt, portMAX_DELAY);
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
    // struct netif *pppif = ppp_netif(pcb);
    // LWIP_UNUSED_ARG(ctx);

    switch(err_code) {
        case PPPERR_NONE: {
            ESP_LOGE(TAG, "status_cb: Connect");
            break;
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
            ESP_LOGE(TAG, "status_cb: User interrupt");
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
}

static int ppp_pause(void) {
    int done = 0;
    int len = 0;
    int ret = EOS_OK;
    char *ok_str = NULL;
    uint64_t t_start;
    uint32_t timeout = 500;
    uint8_t retries = 1;

    xSemaphoreTake(ppp_mutex, portMAX_DELAY);
    eos_modem_flush();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    modem_set_mode(EOS_CELL_UART_MODE_NONE);
    xSemaphoreTake(uart_mutex, portMAX_DELAY);
    eos_modem_write("+++", 3);
    t_start = esp_timer_get_time();

    do {
        len = eos_modem_read(uart_buf + uart_buf_len, sizeof(uart_buf) - uart_buf_len, 10);
        if (len > 0) {
            if (uart_buf_len > 5) {
                ok_str = memstr(uart_buf + uart_buf_len - 5, len + 5, "\r\nOK\r\n");
            } else {
                ok_str = memstr(uart_buf, len + uart_buf_len, "\r\nOK\r\n");
            }
            uart_buf_len += len;
        }
        if (ok_str) {
            pppos_input_tcpip(ppp_handle, (uint8_t *)uart_buf, ok_str - uart_buf);
            ok_str += 6;
            uart_buf_len -= ok_str - uart_buf;
            if (uart_buf_len) memmove(uart_buf, ok_str, uart_buf_len);
            done = 1;
        } else if (uart_buf_len == sizeof(uart_buf)) {
            pppos_input_tcpip(ppp_handle, (uint8_t *)uart_buf, sizeof(uart_buf) / 2);
            memcpy(uart_buf, uart_buf + sizeof(uart_buf) / 2, sizeof(uart_buf) / 2);
            uart_buf_len = sizeof(uart_buf) / 2;
        }
        if (timeout && !done && ((uint32_t)((esp_timer_get_time() - t_start) / 1000) > timeout)) {
            if (!retries) {
                ret = EOS_ERR_TIMEOUT;
                done = 1;
                modem_set_mode(EOS_CELL_UART_MODE_PPP);
                xSemaphoreGive(uart_mutex);
                xSemaphoreGive(ppp_mutex);
            } else {
                retries--;
                eos_modem_write("+++", 3);
                t_start = esp_timer_get_time();
            }
        }
    } while (!done);

    return ret;
}

static int ppp_resume(void) {
    int r;
    int rv = EOS_OK;

    eos_modem_write("ATO\r", 4);
    r = eos_modem_resp("^CONNECT", "^(ERROR|NO CARRIER)", 1000);
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
    xSemaphoreTake(uart_mutex, portMAX_DELAY);

    eos_modem_write(cmd, cmd_len);
    r = eos_modem_resp("^OK", "^ERROR", 1000);
    if (r <= 0) {
        modem_set_mode(uart_mode);
        xSemaphoreGive(uart_mutex);
        return EOS_ERR;
    }

    eos_modem_write("AT+CGDATA=\"PPP\",1\r", 18);
    r = eos_modem_resp("^CONNECT", "^(ERROR|\\+CME ERROR|NO CARRIER)", 1000);
    if (r <= 0) {
        modem_set_mode(uart_mode);
        xSemaphoreGive(uart_mutex);
        return EOS_ERR;
    }

    ppp_handle = pppapi_pppos_create(&ppp_netif, ppp_output_cb, ppp_status_cb, NULL);
    ppp_set_usepeerdns(ppp_handle, 1);
    pppapi_set_default(ppp_handle);
	pppapi_set_auth(ppp_handle, PPPAUTHTYPE_PAP, ppp_user, ppp_pass);
    pppapi_connect(ppp_handle, 0);

    modem_set_mode(EOS_CELL_UART_MODE_PPP);
    xSemaphoreGive(uart_mutex);

    return EOS_OK;
}

static int ppp_disconnect(void) {
    int rv;

    pppapi_close(ppp_handle, 0);

    rv = ppp_pause();
    if (rv) return rv;

    eos_modem_write("ATH\r", 4);
    eos_modem_resp("^OK", NULL, 1000);

    xSemaphoreGive(uart_mutex);
    xSemaphoreGive(ppp_mutex);
    ppp_handle = NULL;

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
    uart_driver_install(UART_NUM_2, UART_SIZE_BUF, UART_SIZE_BUF, 10, &uart_queue, 0);

    // Configuration for the DTR/RI lines
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << UART_GPIO_DTR);
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(UART_GPIO_DTR, 0);

    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
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

    uart_ri_queue = xQueueCreate(4, sizeof(int));
    // Create a task to handle uart event from ISR
    xTaskCreate(uart_event_task, "uart_event", EOS_TASK_SSIZE_UART, NULL, EOS_TASK_PRIORITY_UART, NULL);
    xTaskCreate(uart_ri_event_task, "uart_ri_event", EOS_TASK_SSIZE_UART_RI, NULL, EOS_TASK_PRIORITY_UART_RI, NULL);

    gpio_isr_handler_add(UART_GPIO_RI, uart_ri_isr_handler, NULL);
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
    int done = 0;
    int len = 0;
    int ret = EOS_OK;
    char *ln_end = NULL;
    uint64_t t_start = esp_timer_get_time();

    ln_end = strchr(uart_buf, '\n');

    do {
        if (ln_end == NULL) {
            len = eos_modem_read(uart_buf + uart_buf_len, sizeof(uart_buf) - uart_buf_len - 1, 10);
            if (len > 0) {
                uart_buf[uart_buf_len + len] = '\0';
                ln_end = strchr(uart_buf + uart_buf_len, '\n');
                uart_buf_len += len;
            }
        }
        if (ln_end) {
            ln_end--;
            if ((ln_end >= uart_buf) && (*ln_end == '\r')) {
                if (buf_size > ln_end - uart_buf) {
                    memcpy(buf, uart_buf, ln_end - uart_buf);
                    buf[ln_end - uart_buf] = '\0';
                    ret = EOS_OK;
                } else {
                    ret = EOS_ERR;
                }
                done = 1;
            }
            ln_end += 2;
            uart_buf_len -= ln_end - uart_buf;
            if (uart_buf_len) memmove(uart_buf, ln_end, uart_buf_len);
            ln_end = NULL;
        } else if (uart_buf_len == sizeof(uart_buf) - 1) {
            memcpy(uart_buf, uart_buf + sizeof(uart_buf) / 2, sizeof(uart_buf) / 2);
            uart_buf_len = sizeof(uart_buf) / 2;
        }
        if (timeout && !done && ((uint32_t)((esp_timer_get_time() - t_start) / 1000) > timeout)) {
            ret = EOS_ERR_TIMEOUT;
            done = 1;
        }
    } while (!done);

    return ret;
}

int eos_modem_resp(char *ok_str, char *err_str, uint32_t timeout) {
    int rv;
    regex_t re;
    uint32_t e = 0;
    uint64_t t_start = esp_timer_get_time();

    do {
        rv = eos_modem_readln(at_resp, sizeof(at_resp), timeout - e);
        if (rv) return EOS_ERR;

        if (ok_str) {
            rv = regcomp(&re, ok_str, REG_EXTENDED | REG_NOSUB);
            if (rv) return EOS_ERR;
            if (regexec(&re, at_resp, 0, NULL, 0) == 0) return 1;
        }

        if (err_str) {
            rv = regcomp(&re, err_str, REG_EXTENDED | REG_NOSUB);
            if (rv) return EOS_ERR;
            if (regexec(&re, at_resp, 0, NULL, 0) == 0) return 0;
        }

        at_urc_process(at_resp);
        e = (uint32_t)(esp_timer_get_time() - t_start) / 1000;
        if (e >= timeout) return EOS_ERR;
    } while (1);
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
        if (uart_mode == EOS_CELL_UART_MODE_PPP) rv = ppp_disconnect();
        if (!rv) {
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

int eos_modem_take(void) {
    int rv = EOS_OK;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (uart_mode == EOS_CELL_UART_MODE_PPP) {
        rv = ppp_pause();
    } else {
        modem_set_mode(EOS_CELL_UART_MODE_NONE);
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
    }
    return rv;
}

void eos_modem_give(void) {
    if (uart_mode == EOS_CELL_UART_MODE_PPP) {
        ppp_resume();
    } else {
        modem_set_mode(uart_mode);
        xSemaphoreGive(uart_mutex);
    }
    xSemaphoreGive(mutex);
}

void eos_modem_sleep(void) {
    gpio_set_level(UART_GPIO_DTR, 1);
}

void eos_modem_wake(void) {
    gpio_set_level(UART_GPIO_DTR, 0);
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

int eos_ppp_disconnect(void) {
    int rv = eos_modem_set_mode(EOS_CELL_UART_MODE_ATCMD);

    xSemaphoreTake(mutex, portMAX_DELAY);
    memset(ppp_apn, 0, sizeof(ppp_apn));
    memset(ppp_user, 0, sizeof(ppp_user));
    memset(ppp_pass, 0, sizeof(ppp_pass));
    xSemaphoreGive(mutex);

    return rv;
}