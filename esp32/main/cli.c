#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_system.h"
#include "esp_console.h"

#include "cli.h"
#include "vga.h"
#include "diagnostic_inputs.h"
#include "wifi.h"
#include "telemetry.h"

static const esp_console_repl_config_t repl_config = {
    .max_history_len = 20,
    .history_save_path = NULL,
    .task_stack_size = 10000,
    .task_priority = 1,
    .prompt = NULL,
    .max_cmdline_length = 1000
};

int cli_set_vga_gain(int argc, char *argv[]);

static const esp_console_cmd_t set_gain_command_config = {
    .command = "set_vga_gain",
    .help = "usage: set_vga_gain <gain_num>",
    .hint = NULL,
    .argtable = NULL,
    .func = cli_set_vga_gain
};

int cli_voltage(int argc, char *argv[]);

#define AMP_OUT_INPUT_NAME "amp_out"
#define REFERENCE_INPUT_NAME "reference"
#define VIRTUAL_GROUND_INPUT_NAME "virtual_ground"
static const esp_console_cmd_t read_voltage_command_config = {
    .command = "read_voltage",
    .help = 
        "usage: read_voltage <input>\n input can be "
        AMP_OUT_INPUT_NAME ", "
        REFERENCE_INPUT_NAME ", or "
        VIRTUAL_GROUND_INPUT_NAME,
    .hint = NULL,
    .argtable = NULL,
    .func = cli_voltage
};

int cli_read_adc(int argc, char *argv[]);
static const esp_console_cmd_t read_adc_command_config = {
    .command = "read_adc",
    .help = "usage: read_adc <num_samples>",
    .hint = NULL,
    .argtable = NULL,
    .func = cli_read_adc
};

int cli_connect_wifi(int argc, char *argv[]);
static const esp_console_cmd_t start_wifi_command_config = {
    .command = "start_wifi",
    .help = "Usage: start_wifi <ssid> <password>",
    .hint = NULL,
    .argtable = NULL,
    .func = cli_connect_wifi
};

int cli_transmit_telemetry(int argc, char *argv[]);
static const esp_console_cmd_t start_telemetry_command_config = {
    .command = "transmit_telemetry",
    .help = "Usage: transmit_telemetry <hostname> <service> <num_samples>",
    .hint = NULL,
    .argtable = NULL,
    .func = cli_transmit_telemetry
};

static esp_console_repl_t *repl;

extern QueueHandle_t adc_samples;


int start_cli(void) {
    esp_console_dev_uart_config_t hw_config = 
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_cmd_register(&set_gain_command_config));
    ESP_ERROR_CHECK(esp_console_cmd_register(&read_voltage_command_config));
    ESP_ERROR_CHECK(esp_console_cmd_register(&read_adc_command_config));
    ESP_ERROR_CHECK(esp_console_cmd_register(&start_wifi_command_config));
    ESP_ERROR_CHECK(esp_console_cmd_register(&start_telemetry_command_config));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    return 0;

}

int cli_set_vga_gain(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "error: expecting 1 argument, %d passed instead\n", argc - 1);
        return 1;
    }

    long gain = atol(argv[1]);

    if (gain < 0 || gain > 64 || (gain & (gain - 1)) != 0) {
        fprintf(stderr, "error: gain %ld is not zero or a power of two less than or equal to 64\n", gain);
        return 1;
    }

    return set_vga_gain((unsigned int) gain);
}

int cli_voltage(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "error: expecting 1 argument, %d passed instead\n", argc - 1);
        return 1;
    }

    char *input = argv[1];
    float voltage;
    int error_code;
    if (! strcmp(input, AMP_OUT_INPUT_NAME)) {
        error_code = read_amp_out_voltage(&voltage);
    }
    else if (! strcmp(input, REFERENCE_INPUT_NAME)) {
        error_code = read_reference_voltage(&voltage);
    }
    else if (! strcmp(input, VIRTUAL_GROUND_INPUT_NAME)) {
        error_code = read_vgnd_voltage(&voltage);
    }
    else {
        fprintf(stderr, "Invalid input %s", input);
        return 1;
    }
    
    if (error_code != 0) {
        printf("%f\n", voltage);
        return 0;
    }

    return 1;
}

int cli_read_adc(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "error: expecting 1 argument, %d passed instead\n", argc - 1);
        return 1;
    }

    long num_samples = atol(argv[1]);

    for (int i = 0; i < num_samples; i++) {
        int sample;
        xQueueReceive(adc_samples, &sample, portMAX_DELAY);
        printf("%d\n", sample);
    }
    return 0;
}

int cli_connect_wifi(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "error: expecting 2 argument, %d passed instead\n", argc - 1);
        return 1;
    }

    char *ssid = argv[1];
    char *password = argv[2];

    initialize_wifi(ssid, password);
    return 0;
}

int cli_transmit_telemetry(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "error: expecting 3 argument, %d passed instead\n", argc - 1);
        return 1;
    }

    char *hostname = argv[1];
    char *service = argv[2];
    int num_samples = (int) atol(argv[3]);

    return start_telemetry(hostname, service, adc_samples, num_samples);
}
