#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"

#define TAG "TWAI SLAVE"

#define BTN1_GPIO 4
#define BTN2_GPIO 5
#define TWAI_RX_GPIO CONFIG_EXAMPLE_RX_GPIO_NUM
#define TWAI_TX_GPIO CONFIG_EXAMPLE_TX_GPIO_NUM

#define ID_SLAVE_PING_RESP 0x0B2
#define ID_SLAVE_DATA      0x0B1
#define ID_MASTER_PING     0x0A2

volatile uint32_t pulse_count_btn1 = 0;
volatile uint32_t pulse_count_btn2 = 0;
volatile uint8_t btn1_state = 0;
volatile uint8_t btn2_state = 0;

static void IRAM_ATTR btn1_isr_handler(void* arg) {
    pulse_count_btn1++;
    btn1_state = gpio_get_level(BTN1_GPIO);
}

static void IRAM_ATTR btn2_isr_handler(void* arg) {
    pulse_count_btn2++;
    btn2_state = gpio_get_level(BTN2_GPIO);
}

void twai_slave_task(void *arg) {
    // TWAI setup
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TWAI_TX_GPIO, TWAI_RX_GPIO, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_25KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "Esperando ping del maestro...");
    while (true) {
        twai_message_t rx_msg;
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(1000)) == ESP_OK) {
            if (rx_msg.identifier == ID_MASTER_PING) {
                ESP_LOGI(TAG, "Ping recibido, enviando respuesta...");
                twai_message_t ping_resp = {
                    .identifier = ID_SLAVE_PING_RESP,
                    .data_length_code = 0,
                    .extd = 0, .rtr = 0, .ss = 0
                };
                twai_transmit(&ping_resp, pdMS_TO_TICKS(100));
                break;
            }
        }
    }

    // Setup botones
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN1_GPIO) | (1ULL << BTN2_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN1_GPIO, btn1_isr_handler, NULL);
    gpio_isr_handler_add(BTN2_GPIO, btn2_isr_handler, NULL);

    while (1) {
        uint8_t data[4];
        data[0] = btn1_state;
        data[1] = pulse_count_btn1;
        data[2] = btn2_state;
        data[3] = pulse_count_btn2;

        twai_message_t msg = {
            .identifier = ID_SLAVE_DATA,
            .data_length_code = 4,
            .data = { data[0], data[1], data[2], data[3] },
            .extd = 0, .rtr = 0, .ss = 0
        };

        ESP_LOGI(TAG, "Enviando - BTN1: %d (%d Hz), BTN2: %d (%d Hz)", data[0], data[1], data[2], data[3]);
        twai_transmit(&msg, pdMS_TO_TICKS(100));
        pulse_count_btn1 = 0;
        pulse_count_btn2 = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    xTaskCreatePinnedToCore(twai_slave_task, "twai_slave_task", 4096, NULL, 10, NULL, tskNO_AFFINITY);
}
