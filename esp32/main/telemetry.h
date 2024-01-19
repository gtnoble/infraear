#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

int start_telemetry(char hostname[], char service[], QueueHandle_t adc_samples, int num_readings);