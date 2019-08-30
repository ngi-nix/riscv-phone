#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/i2s.h>
#include <esp_log.h>

#include "eos.h"
#include "modem.h"
#include "fe310.h"

static i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};

#define BUF_SIZE    2048

static QueueHandle_t i2s_queue;

static void i2s_event_task(void *pvParameters) {
    size_t size_out;
    i2s_event_t event;
    // Reserve a buffer and process incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    int first = 1;
    uint8_t *data_first = NULL;

    while (1) {
        // Waiting for I2S event.
        if (xQueueReceive(i2s_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case I2S_EVENT_RX_DONE:
                    // Event of I2S receiving data
                    // printf("*** I2S DATA RECEIVED: %d\n ***", event.size);
                    i2s_read(I2S_NUM_0, (void *)data, BUF_SIZE, &size_out, 1000 / portTICK_RATE_MS);
                    if (first) {
                        if (data_first) {
                            first = 0;
                            i2s_write(I2S_NUM_0, (const void *)data_first, BUF_SIZE, &size_out, 1000 / portTICK_RATE_MS);
                            free(data_first);
                            data_first = NULL;
                        } else {
                            data_first = (uint8_t *) malloc(BUF_SIZE);
                            memcpy(data_first, data, BUF_SIZE);
                        }
                        
                    }
                    i2s_write(I2S_NUM_0, (const void *)data, BUF_SIZE, &size_out, 1000 / portTICK_RATE_MS);
                    break;
                case I2S_EVENT_DMA_ERROR:
                    printf("*** I2S DMA ERROR ***");
                    break;
                default:
                    break;
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

static void i2s_write_task(void *pvParameters) {
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    memset(data, 0x0, BUF_SIZE);

    int i;
    for (i=0; i<BUF_SIZE; i+=16) {
        data[i+3] = 0xaa;
        data[i+2] = 0xff;
    }
    while(1) {
        size_t size_out;
        // i2s_read(I2S_NUM_0, (void *)data, BUF_SIZE, &size_out, 1000 / portTICK_RATE_MS);
        // printf("!! I2S DATA IN: %d\n", size_out);
        i2s_write(I2S_NUM_0, (const void *)data, BUF_SIZE, &size_out, 1000 / portTICK_RATE_MS);
        printf("I2S DATA OUT: %d\n", size_out);
    }
    free(data);
    vTaskDelete(NULL);
}

void eos_pcm_init(void) {
    esp_err_t err;
    
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_SLAVE | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = 32000,
        .bits_per_sample = 32,                                              
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format =  I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_PCM_LONG,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = true,
        .fixed_mclk = 2048000 * 8,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = 33,
        .ws_io_num = 4,
        .data_out_num = 2,
        .data_in_num = 34
    };
    err = i2s_driver_install(I2S_NUM_0, &i2s_config, 10, &i2s_queue);   //install and start i2s driver
    printf("I2S ERR: %d\n", err);
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    printf("I2S ERR: %d\n", err);
    i2s_stop(I2S_NUM_0);
    I2S[I2S_NUM_0]->conf.tx_mono = 1;
    I2S[I2S_NUM_0]->conf.rx_mono = 1;
    i2s_start(I2S_NUM_0);

    // Create a task to handle i2s event from ISR
    xTaskCreate(i2s_event_task, "i2s_event_task", 2048, NULL, EOS_PRIORITY_PCM, NULL);
    // xTaskCreate(i2s_write_task, "i2s_write_task", 2048, NULL, EOS_PRIORITY_PCM, NULL);
}

ssize_t eos_pcm_write(void *data, size_t size) {
    size_t size_out;
    
    esp_err_t ret = i2s_write(I2S_NUM_0, (const void *)data, size, &size_out, portMAX_DELAY);
    if (ret != ESP_OK) return EOS_ERR;
    return size_out;
}

void eos_pcm_call(void) {
    const char *s = "ATD0631942317;\r";

    i2s_zero_dma_buffer(I2S_NUM_0);
    eos_modem_write((void *)s, strlen(s));
    vTaskDelay(1000 / portTICK_RATE_MS);
    i2s_start(I2S_NUM_0);
}

