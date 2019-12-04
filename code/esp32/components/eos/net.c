#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
// #include <freertos/heap_regions.h>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include <driver/spi_slave.h>

#include "eos.h"
#include "msgq.h"
#include "net.h"

static EOSMsgQ net_send_q;
static EOSMsgItem net_sndq_array[EOS_NET_SIZE_BUFQ];

#define SPI_GPIO_RTS        22
#define SPI_GPIO_CTS        21
#define SPI_GPIO_MOSI       23
#define SPI_GPIO_MISO       19
#define SPI_GPIO_SCLK       18
#define SPI_GPIO_CS         5

static SemaphoreHandle_t mutex;

static const char *TAG = "EOS";

static eos_net_fptr_t mtype_handler[EOS_NET_MAX_MTYPE];

static void bad_handler(unsigned char mtype, unsigned char *buffer, uint16_t len) {
    ESP_LOGE(TAG, "NET RECV: bad handler: %d", mtype);
}

static void exchange(void *pvParameters) {
    int repeat = 0;
    unsigned char mtype = 0;
    unsigned char *buffer;
    uint16_t len;
    uint8_t flags;
    unsigned char *buf_send = heap_caps_malloc(EOS_NET_SIZE_BUF, MALLOC_CAP_DMA);
    unsigned char *buf_recv = heap_caps_malloc(EOS_NET_SIZE_BUF, MALLOC_CAP_DMA);

    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.length = EOS_NET_SIZE_BUF*8;
    t.tx_buffer = buf_send;
    t.rx_buffer = buf_recv;
    for (;;) {
        if (!repeat) {
            xSemaphoreTake(mutex, portMAX_DELAY);

            eos_msgq_pop(&net_send_q, &mtype, &buffer, &len, &flags);
            if (mtype) {
                buf_send[0] = ((mtype << 3) | (len >> 8)) & 0xFF;
                buf_send[1] = len & 0xFF;
                if (buffer) {
                    memcpy(buf_send + 2, buffer, len);
                    if (flags & EOS_NET_FLAG_BUF_FREE) free(buffer);
                }
            } else {
                WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1 << SPI_GPIO_RTS));
                buf_send[0] = 0;
                buf_send[1] = 0;
            }

            xSemaphoreGive(mutex);
        }

        memset(buf_recv, 0, EOS_NET_SIZE_BUF);
        spi_slave_transmit(HSPI_HOST, &t, portMAX_DELAY);
        repeat = 0;
        if (buf_recv[0] != 0) {
            mtype = (buf_recv[0] >> 3);
            len = ((buf_recv[0] & 0x07) << 8);
            len |= buf_recv[1];
            buffer = buf_recv + 2;
            if (mtype & EOS_NET_MTYPE_FLAG_ONEW) {
                mtype &= ~EOS_NET_MTYPE_FLAG_ONEW;
                if (buf_send[0]) repeat = 1;
            }
            if (mtype <= EOS_NET_MAX_MTYPE) {
                mtype_handler[mtype-1](mtype, buffer, len);
            } else {
                bad_handler(mtype, buffer, len);
            }
        } else {
            // ESP_LOGI(TAG, "NET RECV NULL");
        }
        // vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// Called after a transaction is queued and ready for pickup by master. We use this to set the handshake line high.
static void _post_setup_cb(spi_slave_transaction_t *trans) {
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, (1 << SPI_GPIO_CTS));
}

// Called after transaction is sent/received. We use this to set the handshake line low.
static void _post_trans_cb(spi_slave_transaction_t *trans) {
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1 << SPI_GPIO_CTS));
}

void eos_net_init(void) {
    esp_err_t ret;

    // Configuration for the handshake lines
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << SPI_GPIO_CTS);
    gpio_config(&io_conf);
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1 << SPI_GPIO_CTS));

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << SPI_GPIO_RTS);
    gpio_config(&io_conf);
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1 << SPI_GPIO_RTS));

    //Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_GPIO_MOSI,
        .miso_io_num = SPI_GPIO_MISO,
        .sclk_io_num = SPI_GPIO_SCLK
    };

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = SPI_GPIO_CS,
        .queue_size = 2,
        .flags = 0,
        .post_setup_cb = _post_setup_cb,
        .post_trans_cb = _post_trans_cb
    };

    //Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected.
    gpio_set_pull_mode(SPI_GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SPI_GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SPI_GPIO_CS, GPIO_PULLUP_ONLY);

    int i;
    for (i=0; i<EOS_NET_MAX_MTYPE; i++) {
        mtype_handler[i] = bad_handler;
    }

    //Initialize SPI slave interface
    ret=spi_slave_initialize(HSPI_HOST, &buscfg, &slvcfg, 1);
    assert(ret==ESP_OK);

    eos_msgq_init(&net_send_q, net_sndq_array, EOS_NET_SIZE_BUFQ);
    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);
    xTaskCreate(&exchange, "net_xchg", 4096, NULL, EOS_IRQ_PRIORITY_NET_XCHG, NULL);
}

int eos_net_send(unsigned char mtype, unsigned char *buffer, uint16_t len, uint8_t flags) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, (1 << SPI_GPIO_RTS));
    int rv = eos_msgq_push(&net_send_q, mtype, buffer, len, flags);
    xSemaphoreGive(mutex);

    return rv;
}

void eos_net_set_handler(unsigned char mtype, eos_net_fptr_t handler) {
    mtype_handler[mtype-1] = handler;
}

