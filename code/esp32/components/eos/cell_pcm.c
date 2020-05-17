#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/i2s.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "msgq.h"
#include "cell.h"

#define PCM_MIC_WM          128
#define PCM_HOLD_CNT_TX     3
#define PCM_HOLD_CNT_RX     3
#define PCM_SIZE_BUFQ       4
#define PCM_SIZE_BUF        (PCM_MIC_WM * 4)

#define PCM_GPIO_BCK        33
#define PCM_GPIO_WS         4
#define PCM_GPIO_DIN        34
#define PCM_GPIO_DOUT       2

#define PCM_ETYPE_WRITE     1

static EOSBufQ pcm_buf_q;
static unsigned char *pcm_bufq_array[PCM_SIZE_BUFQ];

static EOSMsgQ pcm_evt_q;
static EOSMsgItem pcm_evtq_array[PCM_SIZE_BUFQ];
static char pcm_hold_tx;

static i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};

static QueueHandle_t i2s_queue;
static SemaphoreHandle_t mutex;

static const char *TAG = "EOS PCM";

static void i2s_event_task(void *pvParameters) {
    i2s_event_t event;
    unsigned char *buf;
    unsigned char _type;
    size_t bytes_w;
    ssize_t bytes_r;
    uint16_t bytes_e;
    ssize_t hold_bytes_r = 0;
    unsigned char *hold_buf = NULL;
    char hold_cnt = 0;

    while (1) {
        // Waiting for I2S event.
        if (xQueueReceive(i2s_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case I2S_EVENT_RX_DONE:
                    // Event of I2S receiving data
                    if (!hold_cnt) {
                        buf = eos_net_alloc();
                        buf[0] = EOS_CELL_MTYPE_AUDIO;
                        bytes_r = eos_pcm_read(buf + 1, PCM_MIC_WM);
                        eos_net_send(EOS_NET_MTYPE_CELL, buf, bytes_r + 1, 0);
                    } else {
                        hold_cnt--;
                        if (hold_buf == NULL) {
                            hold_buf = eos_net_alloc();
                            hold_buf[0] = EOS_CELL_MTYPE_AUDIO;
                        }
                        if (1 + hold_bytes_r + PCM_MIC_WM <= EOS_NET_SIZE_BUF) hold_bytes_r += eos_pcm_read(hold_buf + 1 + hold_bytes_r, PCM_MIC_WM);
                        if (hold_cnt == 0) {
                            eos_net_send(EOS_NET_MTYPE_CELL, hold_buf, hold_bytes_r + 1, 0);
                            hold_bytes_r = 0;
                            hold_buf = NULL;
                        }
                    }

                    buf = NULL;
                    xSemaphoreTake(mutex, portMAX_DELAY);
                    if (pcm_hold_tx && (eos_msgq_len(&pcm_evt_q) == PCM_HOLD_CNT_TX)) pcm_hold_tx = 0;
                    if (!pcm_hold_tx) eos_msgq_pop(&pcm_evt_q, &_type, &buf, &bytes_e, NULL);
                    xSemaphoreGive(mutex);

                    if (buf) {
                        i2s_write(I2S_NUM_0, (const void *)buf, bytes_e, &bytes_w, portMAX_DELAY);
                        xSemaphoreTake(mutex, portMAX_DELAY);
                        eos_bufq_push(&pcm_buf_q, buf);
                        xSemaphoreGive(mutex);
                    }
                    break;
                case I2S_EVENT_DMA_ERROR:
                    ESP_LOGE(TAG, "*** I2S DMA ERROR ***");
                    break;
                case I2S_EVENT_MAX:
                    hold_cnt = PCM_HOLD_CNT_RX;
                    break;
                default:
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

void eos_pcm_init(void) {
    int i;

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_SLAVE | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = 32000,
        .bits_per_sample = 32,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .dma_buf_count = 4,
        .dma_buf_len = PCM_MIC_WM,
        .use_apll = true,
        .fixed_mclk = 2048000 * 8,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num     = PCM_GPIO_BCK,
        .ws_io_num      = PCM_GPIO_WS,
        .data_in_num    = PCM_GPIO_DIN,
        .data_out_num   = PCM_GPIO_DOUT
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 10, &i2s_queue);   //install and start i2s driver
    i2s_stop(I2S_NUM_0);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    gpio_matrix_in(pin_config.ws_io_num, I2S0I_WS_IN_IDX, 1);
    gpio_matrix_in(pin_config.bck_io_num, I2S0I_BCK_IN_IDX, 1);
    ESP_LOGI(TAG, "TX FIFO:%d TX CHAN:%d RX FIFO:%d RX CHAN:%d", I2S[I2S_NUM_0]->fifo_conf.tx_fifo_mod, I2S[I2S_NUM_0]->conf_chan.tx_chan_mod, I2S[I2S_NUM_0]->fifo_conf.rx_fifo_mod, I2S[I2S_NUM_0]->conf_chan.rx_chan_mod);

    I2S[I2S_NUM_0]->fifo_conf.tx_fifo_mod = 2;
    I2S[I2S_NUM_0]->conf_chan.tx_chan_mod = 0;

    I2S[I2S_NUM_0]->fifo_conf.rx_fifo_mod = 3;
    I2S[I2S_NUM_0]->conf_chan.rx_chan_mod = 1;
    // I2S[I2S_NUM_0]->conf.tx_mono = 1;
    I2S[I2S_NUM_0]->conf.rx_mono = 1;
    // I2S[I2S_NUM_0]->timing.tx_dsync_sw = 1
    // I2S[I2S_NUM_0]->timing.rx_dsync_sw = 1
    // I2S[I2S_NUM_0]->conf.sig_loopback = 0;

    // I2S[I2S_NUM_0]->timing.tx_bck_in_inv = 1;

    eos_msgq_init(&pcm_evt_q, pcm_evtq_array, PCM_SIZE_BUFQ);
    eos_bufq_init(&pcm_buf_q, pcm_bufq_array, PCM_SIZE_BUFQ);
    for (i=0; i<PCM_SIZE_BUFQ; i++) {
        eos_bufq_push(&pcm_buf_q, malloc(PCM_SIZE_BUF));
    }

    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);

    // Create a task to handle i2s event from ISR
    xTaskCreate(i2s_event_task, "i2s_event", EOS_TASK_SSIZE_I2S, NULL, EOS_TASK_PRIORITY_I2S, NULL);
    ESP_LOGI(TAG, "INIT");
}

ssize_t eos_pcm_read(unsigned char *data, size_t size) {
    static unsigned char buf[PCM_SIZE_BUF];
    size_t bytes_r;
    int i;

    if (size > PCM_MIC_WM) return EOS_ERR;

    esp_err_t ret = i2s_read(I2S_NUM_0, (void *)buf, size * 4, &bytes_r, portMAX_DELAY);
    if (ret != ESP_OK) return EOS_ERR;

    for (i=0; i<size/2; i++) {
        data[i * 2] = buf[i * 8 + 3];
        data[i * 2 + 1] = buf[i * 8 + 2];
    }

    return bytes_r / 4;
}

ssize_t eos_pcm_expand(unsigned char *buf, unsigned char *data, size_t size) {
    int i;

    if (size > PCM_MIC_WM) return EOS_ERR;

    memset(buf, 0, PCM_SIZE_BUF);
    for (i=0; i<size/2; i++) {
        buf[i * 8 + 3] = data[i * 2];
        buf[i * 8 + 2] = data[i * 2 + 1];
    }

    return size * 4;
}

int eos_pcm_push(unsigned char *data, size_t size) {
    unsigned char *buf = NULL;
    ssize_t esize;
    int rv;

    if (size > PCM_MIC_WM) return EOS_ERR;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (pcm_hold_tx && (eos_msgq_len(&pcm_evt_q) == PCM_HOLD_CNT_TX)) {
        unsigned char _type;
        uint16_t _len;

        eos_msgq_pop(&pcm_evt_q, &_type, &buf, &_len, NULL);
    } else {
        buf = eos_bufq_pop(&pcm_buf_q);
    }
    xSemaphoreGive(mutex);

    if (buf == NULL) return EOS_ERR_EMPTY;

    esize = eos_pcm_expand(buf, data, size);
    if (esize < 0) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        eos_bufq_push(&pcm_buf_q, buf);
        xSemaphoreGive(mutex);
        return esize;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    rv = eos_msgq_push(&pcm_evt_q, PCM_ETYPE_WRITE, buf, esize, 0);
    if (rv) eos_bufq_push(&pcm_buf_q, buf);
    xSemaphoreGive(mutex);

    return rv;
}

void eos_pcm_start(void) {
    i2s_event_t evt;

    xSemaphoreTake(mutex, portMAX_DELAY);
    while (1) {
        unsigned char _type;
        unsigned char *buf;
        uint16_t len;

        eos_msgq_pop(&pcm_evt_q, &_type, &buf, &len, NULL);
        if (buf) {
            eos_bufq_push(&pcm_buf_q, buf);
        } else {
            break;
        }
    }
    pcm_hold_tx = 1;
    xSemaphoreGive(mutex);

    evt.type = I2S_EVENT_MAX;   /* my type */
    xQueueSend(i2s_queue, &evt, portMAX_DELAY);
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_start(I2S_NUM_0);
}

void eos_pcm_stop(void) {
    i2s_stop(I2S_NUM_0);
}
