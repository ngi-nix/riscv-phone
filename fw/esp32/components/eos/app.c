#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include <driver/spi_slave.h>

#include "eos.h"
#include "msgq.h"
#include "app.h"

#define SPI_GPIO_CTS        26
#define SPI_GPIO_RTS        27
#define SPI_GPIO_MOSI       13
#define SPI_GPIO_MISO       12
#define SPI_GPIO_SCLK       14
#define SPI_GPIO_CS         15

#define SPI_SIZE_BUF        (EOS_APP_SIZE_BUF + 8)
#define SPI_SIZE_HDR        3

static EOSBufQ app_buf_q;
static unsigned char *app_bufq_array[EOS_APP_SIZE_BUFQ];

static EOSMsgQ app_send_q;
static EOSMsgItem app_sndq_array[EOS_APP_SIZE_SNDQ];

static SemaphoreHandle_t mutex;
static SemaphoreHandle_t semaph;
static TaskHandle_t app_xchg_task_handle;
static const char *TAG = "EOS APP";

static eos_app_fptr_t app_handler[EOS_APP_MAX_MTYPE];

static void bad_handler(unsigned char mtype, unsigned char *buffer, uint16_t len) {
    ESP_LOGE(TAG, "bad handler: %d len: %d", mtype, len);
}

// Called after a transaction is queued and ready for pickup by master. We use this to set the handshake line high.
static void _post_setup_cb(spi_slave_transaction_t *trans) {
    gpio_set_level(SPI_GPIO_CTS, 1);
}

// Called after transaction is sent/received. We use this to set the handshake line low.
static void _post_trans_cb(spi_slave_transaction_t *trans) {
    gpio_set_level(SPI_GPIO_CTS, 0);
}

static void app_xchg_task(void *pvParameters) {
    unsigned char mtype = 0;
    unsigned char *buffer;
    uint16_t len;
    unsigned char *buf_send = heap_caps_malloc(SPI_SIZE_BUF, MALLOC_CAP_DMA);
    unsigned char *buf_recv = heap_caps_malloc(SPI_SIZE_BUF, MALLOC_CAP_DMA);
    esp_err_t ret;
    size_t trans_len;

    static spi_slave_transaction_t spi_tr;

    //Configuration for the SPI bus
    static spi_bus_config_t spi_bus_cfg = {
        .mosi_io_num = SPI_GPIO_MOSI,
        .miso_io_num = SPI_GPIO_MISO,
        .sclk_io_num = SPI_GPIO_SCLK
    };

    //Configuration for the SPI slave interface
    static spi_slave_interface_config_t spi_slave_cfg = {
        .mode = 0,
        .spics_io_num = SPI_GPIO_CS,
        .queue_size = 2,
        .flags = 0,
        .post_setup_cb = _post_setup_cb,
        .post_trans_cb = _post_trans_cb
    };

    //Initialize SPI slave interface
    ret = spi_slave_initialize(HSPI_HOST, &spi_bus_cfg, &spi_slave_cfg, 1);
    assert(ret == ESP_OK);

    memset(&spi_tr, 0, sizeof(spi_tr));
    spi_tr.tx_buffer = buf_send;
    spi_tr.rx_buffer = buf_recv;
    spi_tr.length = SPI_SIZE_BUF * 8;

    while (1) {
        xSemaphoreTake(mutex, portMAX_DELAY);

        eos_msgq_pop(&app_send_q, &mtype, &buffer, &len);
        if (mtype) {
            buf_send[0] = mtype;
            buf_send[1] = len >> 8;
            buf_send[2] = len & 0xFF;
            if (buffer) {
                memcpy(buf_send + SPI_SIZE_HDR, buffer, len);
                eos_bufq_push(&app_buf_q, buffer);
                xSemaphoreGive(semaph);
            }
        } else {
            gpio_set_level(SPI_GPIO_RTS, 0);
            buf_send[0] = 0;
            buf_send[1] = 0;
            buf_send[2] = 0;
            len = 0;
        }

        xSemaphoreGive(mutex);

        buf_recv[0] = 0;
        buf_recv[1] = 0;
        buf_recv[2] = 0;
        spi_slave_transmit(HSPI_HOST, &spi_tr, portMAX_DELAY);

        trans_len = spi_tr.trans_len / 8;
        if (trans_len < SPI_SIZE_HDR) continue;

        if (len + SPI_SIZE_HDR > trans_len) {
            spi_tr.tx_buffer = buf_send + trans_len;
            spi_tr.rx_buffer = buf_recv + trans_len;
            spi_tr.length = (SPI_SIZE_BUF - trans_len) * 8;
            spi_slave_transmit(HSPI_HOST, &spi_tr, portMAX_DELAY);
            spi_tr.tx_buffer = buf_send;
            spi_tr.rx_buffer = buf_recv;
            spi_tr.length = SPI_SIZE_BUF * 8;
        }
        mtype = buf_recv[0] & ~EOS_APP_MTYPE_FLAG_MASK;
        len  = (uint16_t)buf_recv[1] << 8;
        len |= (uint16_t)buf_recv[2] & 0xFF;
        buffer = buf_recv + 3;

        if (mtype == 0x00) continue;

        if ((mtype <= EOS_APP_MAX_MTYPE) && (len <= EOS_APP_MTU)) {
            app_handler[mtype - 1](mtype, buffer, len);
        } else {
            bad_handler(mtype, buffer, len);
        }
    }
    vTaskDelete(NULL);
}

void eos_app_init(void) {
    int i;

    // Configuration for the handshake lines
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    io_conf.pin_bit_mask = ((uint64_t)1 << SPI_GPIO_CTS);
    gpio_config(&io_conf);
    gpio_set_level(SPI_GPIO_CTS, 0);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    io_conf.pin_bit_mask = ((uint64_t)1 << SPI_GPIO_RTS);
    gpio_config(&io_conf);
    gpio_set_level(SPI_GPIO_RTS, 0);

    eos_msgq_init(&app_send_q, app_sndq_array, EOS_APP_SIZE_SNDQ);
    eos_bufq_init(&app_buf_q, app_bufq_array, EOS_APP_SIZE_BUFQ);
    for (i=0; i<EOS_APP_SIZE_BUFQ; i++) {
        eos_bufq_push(&app_buf_q, malloc(EOS_APP_SIZE_BUF));
    }

    for (i=0; i<EOS_APP_MAX_MTYPE; i++) {
        app_handler[i] = bad_handler;
    }

    semaph = xSemaphoreCreateCounting(EOS_APP_SIZE_BUFQ, EOS_APP_SIZE_BUFQ);
    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);
    xTaskCreate(&app_xchg_task, "app_xchg", EOS_TASK_SSIZE_APP_XCHG, NULL, EOS_TASK_PRIORITY_APP_XCHG, &app_xchg_task_handle);
    ESP_LOGI(TAG, "INIT");
}

unsigned char *eos_app_alloc(void) {
    unsigned char *ret;

    xSemaphoreTake(semaph, portMAX_DELAY);
    xSemaphoreTake(mutex, portMAX_DELAY);
    ret = eos_bufq_pop(&app_buf_q);
    xSemaphoreGive(mutex);

    return ret;
}

void eos_app_free(unsigned char *buf) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    eos_bufq_push(&app_buf_q, buf);
    xSemaphoreGive(semaph);
    xSemaphoreGive(mutex);
}

int eos_app_send(unsigned char mtype, unsigned char *buffer, uint16_t len) {
    int rv = EOS_OK;

    xSemaphoreTake(mutex, portMAX_DELAY);
    gpio_set_level(SPI_GPIO_RTS, 1);
    rv = eos_msgq_push(&app_send_q, mtype, buffer, len);
    xSemaphoreGive(mutex);

    if (rv) eos_app_free(buffer);

    return rv;
}

void eos_app_set_handler(unsigned char mtype, eos_app_fptr_t handler) {
    if (handler == NULL) handler = bad_handler;
    if (mtype && (mtype <= EOS_APP_MAX_MTYPE)) app_handler[mtype - 1] = handler;
}
