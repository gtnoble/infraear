
#include <stdio.h>
#include <assert.h>  

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#include "diagnostic_inputs.h"

#define ADC_CHANNEL_ATTENUATION ADC_ATTEN_DB_12


typedef struct {
    adc_oneshot_unit_handle_t adc_unit;
    adc_cali_handle_t calibration;
    const adc_cali_line_fitting_config_t calibration_config;
    const adc_oneshot_unit_init_cfg_t initial_config;
    const char* description;
} Adc;

typedef struct {
    const adc_channel_t adc_channel;
    const gpio_num_t gpio_pin;
    const char *input_description;
    const adc_oneshot_chan_cfg_t *channel_config;
    const Adc *adc;
} DiagnosticInput;

static const adc_oneshot_chan_cfg_t adc_channel_config = {
    .atten = ADC_CHANNEL_ATTENUATION,
    .bitwidth = ADC_BITWIDTH_DEFAULT
};

static Adc adc_1 = {
    .calibration_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_CHANNEL_ATTENUATION,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    },
    .initial_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode =  ADC_ULP_MODE_DISABLE
    },
    .description = "ADC1"
};

static Adc adc_2 = {
    .calibration_config = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_CHANNEL_ATTENUATION,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    },
    .initial_config = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode =  ADC_ULP_MODE_DISABLE
    },
    .description = "ADC2"
}; 

static Adc *adcs[] = {
    &adc_1,
    &adc_2
};

static const DiagnosticInput amp_out_input = {
    .adc_channel = ADC_CHANNEL_9,
    .gpio_pin = GPIO_NUM_26,
    .input_description = "amp out",
    .channel_config = &adc_channel_config,
    .adc = &adc_2
};

static const DiagnosticInput vgnd_input = {
    .adc_channel = ADC_CHANNEL_7,
    .gpio_pin = GPIO_NUM_27,
    .input_description = "virtual ground",
    .channel_config = &adc_channel_config,
    .adc = &adc_2
};

static const DiagnosticInput reference_input = {
    .adc_channel = ADC_CHANNEL_6,
    .gpio_pin = GPIO_NUM_14,
    .input_description = "reference voltage",
    .channel_config = &adc_channel_config,
    .adc = &adc_2
};

static const DiagnosticInput *diagnostic_inputs[] = {
    &amp_out_input,
    &vgnd_input,
    &reference_input
};

static const char *TAG = "diagnostic_inputs";

int read_voltage(const DiagnosticInput *input, float *out_voltage);

int initialize_diagnostic_inputs(void) {

    ESP_LOGD(TAG, "Initializing ADCs");
    for (int i = 0; i < sizeof(adcs) / sizeof(const Adc *); i++) {
        Adc *adc = adcs[i];
        ESP_LOGD(TAG, "Initializing ADC %s", adc->description);

        esp_err_t error;
        error = adc_oneshot_new_unit(&adc->initial_config, &adc->adc_unit);
        if (error != ESP_OK) {
            ESP_LOGE(
                TAG, 
                "Failed to initialize ADC %s. details: %s", 
                adc->description, 
                esp_err_to_name(error)
            );
            return 1;
        }

        error = adc_cali_create_scheme_line_fitting(&adc->calibration_config, &adc->calibration);
        if (error != ESP_OK) {
            ESP_LOGE(
                TAG, 
                "Failed to calibrate ADC %s. details: %s", 
                adc->description, 
                esp_err_to_name(error)
            );
            return 1;
        }
    }

    ESP_LOGD(TAG, "Initializing inputs");
    for (int i = 0; i < sizeof(diagnostic_inputs) / sizeof(const DiagnosticInput *); i++) {
        const DiagnosticInput *input = diagnostic_inputs[i];
        ESP_LOGD(TAG, "Initializing input %s", input->input_description);

        esp_err_t error = gpio_set_direction(input->gpio_pin, GPIO_MODE_INPUT);
        if (error != ESP_OK) {
            ESP_LOGE(
                TAG, 
                "Failed to set %s pin to input. details: %s", 
                input->input_description, 
                esp_err_to_name(error)
            );
            return 1;
        }

        error = adc_oneshot_config_channel(input->adc->adc_unit, input->adc_channel, input->channel_config);
        if (error != ESP_OK) {
            ESP_LOGE(
                TAG, 
                "Failed to configure %s input channel. details: %s", 
                input->input_description, 
                esp_err_to_name(error)
            );
            return 1;
        }
    }

    return 0;
}

int read_voltage(const DiagnosticInput *input, float *out_voltage) {
    ESP_LOGD(TAG, "Reading voltage from input %s", input->input_description);

    int raw_reading;
    esp_err_t error;
    error = adc_oneshot_read(input->adc->adc_unit, input->adc_channel, &raw_reading);
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG, 
            "Failed to read from input %s: details: %s", 
            input->input_description, 
            esp_err_to_name(error)
        );
        return 1;
    }

    int voltage_millivolts;
    error = adc_cali_raw_to_voltage(input->adc->calibration, raw_reading, &voltage_millivolts);
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to apply calibration to reading from ADC %s. details: %s",
            input->adc->description,
            esp_err_to_name(error)
        );
        return 1;
    }

    *out_voltage = (float) voltage_millivolts / 1000.0;
    return 1;
}

int read_vgnd_voltage(float *out_voltage) {
    return read_voltage(&vgnd_input, out_voltage);
}

int read_amp_out_voltage(float *out_voltage) {
    return read_voltage(&amp_out_input, out_voltage);
}

int read_reference_voltage(float *out_voltage) {
    return read_voltage(&reference_input, out_voltage);
}
