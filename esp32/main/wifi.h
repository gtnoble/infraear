#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void initialize_wifi(const char ssid[], const char password[], SemaphoreHandle_t *wifi_connected);