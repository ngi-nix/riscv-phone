#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>

#include "eos.h"
#include "msgq.h"
#include "net.h"
#include "cell.h"

#define CELL_SIZE_QUEUE     2

static const char *TAG = "EOS CELL";

static uint8_t cell_mode;

static EOSBufQ cell_buf_q;
static unsigned char *cell_bufq_array[CELL_SIZE_QUEUE];

static SemaphoreHandle_t mutex;
static QueueHandle_t cell_queue;

static void _cell_handler(unsigned char _mtype, unsigned char *buffer, uint16_t size) {
    uint8_t mtype;

    if (size < 1) return;
    mtype = buffer[0];
    switch (mtype & EOS_CELL_MTYPE_MASK) {
        case EOS_CELL_MTYPE_DEV:
            switch (mtype & ~EOS_CELL_MTYPE_MASK) {
                case EOS_CELL_MTYPE_RESET:
                    eos_modem_reset();
                    break;

                case EOS_CELL_MTYPE_UART_DATA:
                    if (eos_modem_get_mode() == EOS_CELL_UART_MODE_RELAY) eos_modem_write(buffer+1, size-1);
                    break;

                case EOS_CELL_MTYPE_UART_TAKE:
                    cell_mode = eos_modem_get_mode();
                    eos_modem_set_mode(EOS_CELL_UART_MODE_RELAY);
                    break;

                case EOS_CELL_MTYPE_UART_GIVE:
                    eos_modem_set_mode(cell_mode);
                    break;

                case EOS_CELL_MTYPE_PCM_DATA:
                    eos_cell_pcm_push(buffer+1, size-1);
                    break;

                case EOS_CELL_MTYPE_PCM_START:
                    eos_cell_pcm_start();
                    break;

                case EOS_CELL_MTYPE_PCM_STOP:
                    eos_cell_pcm_stop();
                    break;
            }
            break;

        case EOS_CELL_MTYPE_VOICE:
            eos_cell_voice_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;

        case EOS_CELL_MTYPE_SMS:
            eos_cell_sms_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;

        case EOS_CELL_MTYPE_USSD:
            eos_cell_ussd_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;

        case EOS_CELL_MTYPE_DATA:
            eos_cell_data_handler(mtype & ~EOS_CELL_MTYPE_MASK, buffer, size);
            break;
    }
}

static void cell_handler_task(void *pvParameters) {
    EOSMsgItem mi;

    while (1) {
        if (xQueueReceive(cell_queue, &mi, portMAX_DELAY)) {
            _cell_handler(mi.type, mi.buffer, mi.len);
            xSemaphoreTake(mutex, portMAX_DELAY);
            eos_bufq_push(&cell_buf_q, mi.buffer);
            xSemaphoreGive(mutex);
        }
    }
    vTaskDelete(NULL);
}

static void cell_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    EOSMsgItem mi;
    unsigned char *buf;

    xSemaphoreTake(mutex, portMAX_DELAY);
    buf = eos_bufq_pop(&cell_buf_q);
    xSemaphoreGive(mutex);

    if (buf == NULL) {
        ESP_LOGE(TAG, "Cell message NOT handled: %2x", type);
        return;
    }

    memcpy(buf, buffer, len);
    mi.type = type;
    mi.buffer = buf;
    mi.len = len;
    xQueueSend(cell_queue, &mi, portMAX_DELAY);
}

void eos_cell_init(void) {
    int i;

    eos_bufq_init(&cell_buf_q, cell_bufq_array, CELL_SIZE_QUEUE);
    for (i=0; i<CELL_SIZE_QUEUE; i++) {
        eos_bufq_push(&cell_buf_q, malloc(EOS_NET_SIZE_BUF));
    }

    mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(mutex);
    cell_queue = xQueueCreate(CELL_SIZE_QUEUE, sizeof(EOSMsgItem));
    xTaskCreate(cell_handler_task, "cell_handler", EOS_TASK_SSIZE_CELL, NULL, EOS_TASK_PRIORITY_CELL, NULL);

    eos_net_set_handler(EOS_NET_MTYPE_CELL, cell_handler);
}

