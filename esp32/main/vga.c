#include "esp_log.h"

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "driver/gpio.h"
#include "vga.h"

#define VGA_G0_PIN GPIO_NUM_13

#define VGA_G1_PIN GPIO_NUM_12 

#define VGA_G2_PIN GPIO_NUM_2

static const gpio_num_t vga_pins[] = {VGA_G0_PIN, VGA_G1_PIN, VGA_G2_PIN};

static const char *TAG = "VGA";

int set_vga_gain(unsigned int gain) {
    assert(gain <= 64);
    assert((gain & (gain - 1)) == 0);

    ESP_LOGI(TAG, "Setting VGA gain level to %u", gain);

    unsigned int gain_code;
    switch (gain) {
        case 0: gain_code = 0b000; break;
        case 1: gain_code = 0b001; break;
        case 2: gain_code = 0b010; break;
        case 4: gain_code = 0b011; break;
        case 8: gain_code = 0b100; break;
        case 16: gain_code = 0b101; break;
        case 32: gain_code = 0b110; break;
        case 64: gain_code = 0b111; break;
        default: assert(false);
    }




    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "Setting G%d pin to output", i);
        esp_err_t error = gpio_set_direction(vga_pins[i], GPIO_MODE_OUTPUT);
        if (error != ESP_OK) {
            ESP_LOGE(
                TAG, 
                "Error setting G%d pin to output. Details: %s", 
                i,
                esp_err_to_name(error)
            );
            return 1;
        }

        uint32_t level = (gain_code >> i) & 1;
        ESP_LOGD(TAG, "Setting G%d pin to %lu", i, level);
        error = gpio_set_level(vga_pins[i], level);
        if (error != ESP_OK) {
            ESP_LOGE(
                TAG, 
                "Error setting G%d pin level. Details: %s", 
                i, 
                esp_err_to_name(error));
            return 1;
        }
    }

    return 0;
}