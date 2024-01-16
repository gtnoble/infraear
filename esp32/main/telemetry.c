#include <stdio.h>

#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char* TAG = "telemetry";

#define TELEMETRY_MESSAGE_LENGTH 100

int start_telemetry(char hostname[], char service[], QueueHandle_t adc_samples, int num_readings) {
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo *servinfo;

    ESP_LOGI(TAG, "Requesting address info for %s", hostname);
    int error = getaddrinfo(hostname, service, &hints, &servinfo);
    if (error != 0) {
        ESP_LOGE(
            TAG, 
            "Could not get address parameters for %s. error code: %d",
            hostname,
            error
        );
            return 1;
    }

    ESP_LOGI(TAG, "Opening a UDP telemetry socket to %s", hostname);
    int telemetry_sd = socket(
        servinfo->ai_family, 
        servinfo->ai_socktype, 
        servinfo->ai_protocol
    );

    ESP_LOGI(TAG, "Beginning transmission of telemetry data");
    for (int i = 0; i < num_readings; i++) {
        int sample;
        xQueueReceive(adc_samples, &sample, portMAX_DELAY);
        char reading_data[TELEMETRY_MESSAGE_LENGTH];
        snprintf(reading_data, TELEMETRY_MESSAGE_LENGTH, "adcReading:%d|t", sample);

        ESP_LOGD(TAG, "Sending telemetry reading packet: \"%s\"", reading_data);
        int bytes_sent = sendto(
            telemetry_sd, 
            reading_data, 
            TELEMETRY_MESSAGE_LENGTH, 
            0, 
            servinfo->ai_addr,
            sizeof(struct sockaddr_storage)
        );
        if (bytes_sent == -1) {
            ESP_LOGE(TAG, "Failed to send a telemetry reading packet!");
        }


    }

    freeaddrinfo(servinfo);

    return 0;
}

