#include <stdio.h>

#include "esp_log.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "cli.h"
#include "adc.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "diagnostic_inputs.h"

#define SAMPLE_QUEUE_LENGTH 100
QueueHandle_t adc_samples;

SemaphoreHandle_t wifi_is_connected;


void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialize_diagnostic_inputs();

    adc_samples = xQueueCreate(SAMPLE_QUEUE_LENGTH, sizeof(int));
    initialize_adc(&adc_samples);

    wifi_is_connected = xSemaphoreCreateBinary();
    initialize_wifi("ssid", "password", &wifi_is_connected);

    start_cli();


    for ( ;; ) {
    vTaskDelay( 200 );
    }

}

