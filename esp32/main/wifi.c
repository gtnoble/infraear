#include <assert.h>
#include <string.h>
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "wifi.h"


static const char* TAG = "wifi";

wifi_config_t station_config;
esp_netif_t *esp_netif;

esp_event_loop_handle_t event_loop;
extern bool wifi_is_connected;

static void on_station_start(void *context, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void on_wifi_connected(void *context, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void on_wifi_disconnected(void *context, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void on_got_ip(void *wifi_connected_semaphore, esp_event_base_t event_base, int32_t event_id, void *event_data);

void initialize_wifi(const char ssid[], const char password[]) {
    assert(strlen(ssid) <= 32);
    assert(strlen(password) <= 64);

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    strncpy((char *) station_config.sta.ssid, ssid, 32);
    ESP_LOGI(TAG, "SSID: \"%s\"", station_config.sta.ssid);
    strncpy((char *) station_config.sta.password, password, 64);
    ESP_LOGI(TAG, "PASS: \"%s\"", station_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station_config));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, on_station_start, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, on_wifi_connected, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_wifi_disconnected, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

}


static void on_station_start(void *context, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Wifi station started!");
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void on_wifi_connected(void *context, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Connected to wifi!");
}

static void on_wifi_disconnected(void *context, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Wifi disconnected! Attempting to reconnect...");
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void on_got_ip(void *context, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "Receieved an IP Address.");
    wifi_is_connected = true;
}