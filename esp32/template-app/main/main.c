#include <stdio.h>
#include "esp_log.h"
#include "diagnostic_inputs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cli.h"
#include "adc.h"

void app_main(void)
{
    initialize_diagnostic_inputs();
    start_adc_clock();
    start_cli();


    for ( ;; ) {
    vTaskDelay( 200 );
    }

}

