#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/syscon_reg.h"
#include "soc/rtc.h"

#include "hal/spi_types.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

#include "esp_intr_alloc.h"

#include "adc.h"

#define ADC_CLOCK_PIN GPIO_NUM_0

#define IDEAL_ADC_CLOCK_FREQ 16.384E6
#define DATA_READY_PIN GPIO_NUM_34

typedef struct {
    unsigned int gpio_num;
    unsigned int iomux_signal;
    bool is_input;
    bool invert_output;
} IoMuxPinConfig;

typedef struct {
    IoMuxPinConfig sclk_pin;
    IoMuxPinConfig miso_pin;
    IoMuxPinConfig mosi_pin;
    spi_host_device_t spi_host;
} SpiBusConfig;

typedef struct {
    const SpiBusConfig *bus_config;
    spi_device_interface_config_t interface_configuration;
} SpiDeviceConfig;

const SpiBusConfig adc_bus_config = {
    .sclk_pin = {
        .gpio_num = GPIO_NUM_18,
        .iomux_signal = FUNC_GPIO18_VSPICLK,
        .is_input = false,
        .invert_output = false
    },
    .miso_pin = {
        .gpio_num = GPIO_NUM_19,
        .iomux_signal = VSPIQ_IN_IDX,
        .is_input = true
    },
    .mosi_pin = {
        .gpio_num = GPIO_NUM_23,
        .iomux_signal = FUNC_GPIO23_VSPID,
        .is_input = false,
        .invert_output = false
    },
    .spi_host = SPI3_HOST,
};

const SpiDeviceConfig adc_device_config = {
    .bus_config = &adc_bus_config,
    .interface_configuration = {
        .command_bits = 2,
        .address_bits = 6,
        .dummy_bits = 0,
        .mode = 3,
        .clock_speed_hz = 20000000,
        .spics_io_num = GPIO_NUM_5,
        .queue_size = 5
    }
};



const spi_bus_config_t adc_spi_bus_config = {
    .mosi_io_num = -1,
    .miso_io_num = -1,
    .sclk_io_num = -1,
    .quadhd_io_num = -1,
    .data2_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .max_transfer_sz = 0,
    .flags = 0,
    .intr_flags = 0
};

const uint16_t read_command = 0b01;
const uint16_t write_command = 0b00; 

const uint64_t digital_filter_register = 0x19;
const uint8_t digital_filter_type = 0b100 << 4;
const uint8_t digital_filter_decimation_rate = 0b111;

spi_transaction_t set_digital_filter_transaction = {
    .flags = SPI_TRANS_USE_TXDATA,
    .cmd = write_command,
    .addr = digital_filter_register,
    .length = 8,
    .rxlength = 8,
    .tx_data = {digital_filter_type | digital_filter_decimation_rate, 0, 0, 0}
};

const uint64_t conversion_result_register = 0x2c;
spi_transaction_t read_adc_transaction = {
    .flags = SPI_TRANS_USE_RXDATA,
    .cmd = read_command,
    .addr = conversion_result_register,
    .length = 24,
};

static const char *TAG = "ADC";

spi_device_handle_t adc_device;

void start_adc_clock(void);
int initialize_spi_bus(const SpiBusConfig *bus_config);
int initialize_device_spi(SpiDeviceConfig device_config, spi_device_handle_t *device);
void initialize_iomux_pin(IoMuxPinConfig pin_config);
void data_ready_isr(void *adc_samples);
int start_collecting_samples(QueueHandle_t *adc_samples);

/**
 * @brief
 * Initialize the external ADC and begin collecting samples. Sample data is stored in a queue.
 * @param adc_samples Queue of `int` to store samples in. 
 * @return 0 if success
 */
int initialize_adc(QueueHandle_t *adc_samples) {
    start_adc_clock();
    if (initialize_spi_bus(adc_device_config.bus_config)) {
        return 1;
    }
    if (initialize_device_spi(adc_device_config, &adc_device)) {
        return 1;
    };

    esp_err_t error = spi_device_polling_transmit(adc_device, &set_digital_filter_transaction);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ADC digital filter params. details: %s", esp_err_to_name(error));
        return 1;
    }

    error = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR Service. details: %s", esp_err_to_name(error));
        return 1;
    }

    start_collecting_samples(adc_samples);
    return 0;
}

/**
 * @brief 
 * Start the APLL ESP32 clock
 */
void start_adc_clock(void) {
    ESP_LOGD(TAG, "Initializing ADC Clock Pin");
    gpio_iomux_out(ADC_CLOCK_PIN, FUNC_GPIO0_CLK_OUT1, false);

    // Output APLL clock to CLK_OUT1
    ESP_LOGD(TAG, "Setting clock pin to APLL output");
    REG_WRITE(PIN_CTRL, 0x6);

    ESP_LOGD(TAG, "Enabling APLL clock");
    rtc_clk_apll_enable(true);

    uint32_t odiv, sdm2, sdm1, sdm0;
    ESP_LOGD(TAG, "Calculating APLL coefficients");
    uint32_t actual_adc_clock_frequency = rtc_clk_apll_coeff_calc(
        (uint32_t) IDEAL_ADC_CLOCK_FREQ, 
        &odiv, 
        &sdm0, 
        &sdm1, 
        &sdm2
    );
    ESP_LOGD(TAG, 
        "Ideal APLL clock frequency: %d\n"
        "Actual APLL clock frequency: %d\n"
        "Calculated APLL coefficients: "
        "sdm0 = %d, sdm1 = %d, sdm2 = %d, odiv = %d",
        (int) IDEAL_ADC_CLOCK_FREQ,
        (int) actual_adc_clock_frequency,
        (int) sdm0,
        (int) sdm1,
        (int) sdm2,
        (int) odiv
    );

    ESP_LOGD(TAG, "Setting APLL coefficients");
    rtc_clk_apll_coeff_set(odiv, sdm0, sdm1, sdm2);
}

/**
 * @brief 
 * Initialize the SPI bus that connects to a device
 * @param bus_config 
 * @return 0 if success 
 */
int initialize_spi_bus(const SpiBusConfig *bus_config) {
    ESP_LOGD(TAG, "Initializing SCLK Pin");
    initialize_iomux_pin(bus_config->sclk_pin);
    ESP_LOGD(TAG, "Initializing MOSI Pin");
    initialize_iomux_pin(bus_config->mosi_pin);
    ESP_LOGD(TAG, "Initializing MISO Pin");
    initialize_iomux_pin(bus_config->miso_pin);
    ESP_LOGD(TAG, "Initializing SPI Bus");
    esp_err_t error = spi_bus_initialize(bus_config->spi_host, &adc_spi_bus_config, SPI_DMA_CH_AUTO);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus. Details: %s", esp_err_to_name(error));
        return 1;
    }
    return 0;
}

/**
 * @brief 
 * Initialize an SPI device and add to the bus
 * @param device_config 
 * @param device 
 * @return 0 if success 
 */
int initialize_device_spi(SpiDeviceConfig device_config, spi_device_handle_t *device) {
    ESP_LOGD(TAG, "Initializing Chip Select Pin");
    esp_err_t error = gpio_set_direction(device_config.interface_configuration.spics_io_num, GPIO_MODE_OUTPUT);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set chip select pin to output. details: %s", esp_err_to_name(error));
        return 1;
    }
    ESP_LOGD(TAG, "Adding device to SPI bus.");
    error = spi_bus_add_device(device_config.bus_config->spi_host, &device_config.interface_configuration, device);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to SPI bus. details: %s", esp_err_to_name(error));
        return 1;

    }
    return 0;
}

void initialize_iomux_pin(IoMuxPinConfig pin_config) {
    if (pin_config.is_input) {
        gpio_iomux_in(pin_config.gpio_num, pin_config.iomux_signal);
    }
    else {
        gpio_iomux_out(
            pin_config.gpio_num, pin_config.iomux_signal, pin_config.invert_output);
    }
}

int start_collecting_samples(QueueHandle_t *adc_samples) {

    esp_err_t error;
    error = gpio_set_direction(DATA_READY_PIN, GPIO_MODE_INPUT);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ADC data ready pin to input. details: %s", esp_err_to_name(error));
        return 1;
    }

    error = gpio_set_intr_type(DATA_READY_PIN, GPIO_INTR_POSEDGE);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ADC data ready pin to trigger on positive edge. details: %s", esp_err_to_name(error));
        return 1;
    }

    error = gpio_isr_handler_add(DATA_READY_PIN, data_ready_isr, adc_samples);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach ADC read ISR handler. details: %s", esp_err_to_name(error));
        return 1;
    }

    error = gpio_intr_enable(DATA_READY_PIN);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ADC data ready interrupt. details: %s", esp_err_to_name(error));
        return 1;
    }
    return 0;
}

void data_ready_isr(void *adc_samples) {
    spi_device_polling_transmit(adc_device, &read_adc_transaction);
    uint8_t *adc_bytes = read_adc_transaction.rx_data;

    int sample =
        ((int) 
        (((uint32_t) adc_bytes[0]) +
        (((uint32_t) adc_bytes[1]) << 8) +
        (((uint32_t) adc_bytes[2]) << 16))) - 
        (1 << 23);

    xQueueSendToBackFromISR(*((QueueHandle_t *) adc_samples), &sample, NULL);
}
