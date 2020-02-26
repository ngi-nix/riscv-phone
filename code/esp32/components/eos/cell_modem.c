#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "cell.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

#define UART_BUF_SIZE   1024

#define UART_GPIO_TXD   16
#define UART_GPIO_RXD   17
#define UART_GPIO_DTR   32
#define UART_GPIO_RI    35

static QueueHandle_t uart_queue;
static QueueHandle_t uart_ri_queue;

static const char *TAG = "EOS MODEM";

static void uart_event_task(void *pvParameters) {
    char mode = EOS_CELL_UART_MODE_NONE;
    uart_event_t event;
    size_t len;
    unsigned char *buf;

    while (1) {
        /* Waiting for UART event.
         */
        if (xQueueReceive(uart_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    /* Event of UART receiving data
                     */
                    switch (mode) {
                        case EOS_CELL_UART_MODE_PPP:
                            break;

                        case EOS_CELL_UART_MODE_RELAY:
                            buf = eos_net_alloc();
                            buf[0] = EOS_CELL_MTYPE_DATA;
                            len = uart_read_bytes(UART_NUM_2, buf+1, MIN(event.size, EOS_NET_SIZE_BUF-1), 100 / portTICK_RATE_MS);
                            eos_net_send(EOS_NET_MTYPE_CELL, buf, len+1, 0);
                            break;

                        default:
                            break;
                    }
                    break;

                case UART_EVENT_MAX:
                    /* Mode change
                     */
                    mode = (char)event.size;
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
        if (xQueueReceive(uart_ri_queue, (void * )&level, (portTickType)portMAX_DELAY) && (level == 0)) {
            uint64_t t_start = esp_timer_get_time();
            if (xQueueReceive(uart_ri_queue, (void * )&level, 200 / portTICK_RATE_MS) && (level == 1)) {
                uint64_t t_end = esp_timer_get_time();
                ESP_LOGI(TAG, "TDELTA:%u", (uint32_t)(t_end - t_start));
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
    uart_driver_install(UART_NUM_2, UART_BUF_SIZE, UART_BUF_SIZE, 10, &uart_queue, 0);

    // Configuration for the DTR/RI lines
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << UART_GPIO_DTR);
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(UART_GPIO_DTR, 1);

    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << UART_GPIO_RI);
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    uart_ri_queue = xQueueCreate(4, sizeof(int));
    // Create a task to handle uart event from ISR
    xTaskCreate(uart_event_task, "uart_event", EOS_TASK_SSIZE_UART, NULL, EOS_TASK_PRIORITY_UART, NULL);
    xTaskCreate(uart_ri_event_task, "uart_ri_event", EOS_TASK_SSIZE_UART_RI, NULL, EOS_TASK_PRIORITY_UART_RI, NULL);

    gpio_isr_handler_add(UART_GPIO_RI, uart_ri_isr_handler, NULL);
    ESP_LOGI(TAG, "INIT");
}

ssize_t eos_modem_write(void *data, size_t size) {
    return uart_write_bytes(UART_NUM_2, (const char *)data, size);
}

void eos_modem_set_mode(char mode) {
    uart_event_t evt;

    evt.type = UART_EVENT_MAX;  /* my type */
    evt.size = mode;
    xQueueSend(uart_queue, (void *)&evt, portMAX_DELAY);
}
