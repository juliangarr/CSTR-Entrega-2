#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "esp_log.h"

#define TAG "TWAI MASTER"

#define TWAI_RX_GPIO CONFIG_EXAMPLE_RX_GPIO_NUM
#define TWAI_TX_GPIO CONFIG_EXAMPLE_TX_GPIO_NUM

#define ID_MASTER_PING     0x0A2
#define ID_SLAVE_PING_RESP 0x0B2
#define ID_SLAVE_DATA      0x0B1

void twai_master_task(void *arg) {
    // TWAI setup
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TWAI_TX_GPIO, TWAI_RX_GPIO, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_25KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    // Asegurar conexión (Handshake)
    ESP_LOGI(TAG, "Enviando ping al esclavo...");
    twai_message_t ping = {
        .identifier = ID_MASTER_PING,
        .data_length_code = 0,
        .extd = 0, .rtr = 0, .ss = 1
    };

    while (true) {
        twai_transmit(&ping, pdMS_TO_TICKS(100));
        twai_message_t rx_msg;
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(500)) == ESP_OK &&
            rx_msg.identifier == ID_SLAVE_PING_RESP) {
            ESP_LOGI(TAG, "Esclavo respondió al ping");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    // Recepción periódica
    while (1) {
        twai_message_t rx_msg;
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(1100)) == ESP_OK) {
            if (rx_msg.identifier == ID_SLAVE_DATA && rx_msg.data_length_code == 4) {
                uint8_t btn1_state = rx_msg.data[0];
                uint8_t btn1_freq = rx_msg.data[1];
                uint8_t btn2_state = rx_msg.data[2];
                uint8_t btn2_freq = rx_msg.data[3];
                
                // Mostrar estado de ls pulsadores y frecuencia
                ESP_LOGI(TAG, "BTN1: %s (%d Hz), BTN2: %s (%d Hz)",
                    btn1_state ? "ON" : "OFF", btn1_freq,
                    btn2_state ? "ON" : "OFF", btn2_freq);
            }
        }
    }
}

void app_main(void) {
    xTaskCreatePinnedToCore(twai_master_task, "twai_master_task", 4096, NULL, 10, NULL, tskNO_AFFINITY);
}
