#include <stdio.h>
#include "esp_log.h"
#include "diagnostic_inputs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cli.h"
#include "adc.h"

#define SAMPLE_QUEUE_LENGTH 100
QueueHandle_t adc_samples;

void app_main(void)
{
    initialize_diagnostic_inputs();

    adc_samples = xQueueCreate(SAMPLE_QUEUE_LENGTH, sizeof(int));

    initialize_adc(&adc_samples);
    start_cli();


    for ( ;; ) {
    vTaskDelay( 200 );
    }

}

