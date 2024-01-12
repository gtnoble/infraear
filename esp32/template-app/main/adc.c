#include "esp_log.h"

#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/syscon_reg.h"
#include "soc/rtc.h"

#include "adc.h"

#define ADC_CLOCK_PIN GPIO_NUM_0

#define IDEAL_ADC_CLOCK_FREQ 16.384E6

static const char *TAG = "ADC";

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