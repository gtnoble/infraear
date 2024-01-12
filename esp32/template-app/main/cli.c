#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "cli.h"
#include "esp_system.h"
#include "esp_console.h"

#include "vga.h"
#include "diagnostic_inputs.h"

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

static esp_console_repl_t *repl;

static const char *TAG = "CLI";


int start_cli(void) {
    esp_err_t error;
    esp_console_dev_uart_config_t hw_config = 
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    error = esp_console_new_repl_uart(
        &hw_config, &repl_config, &repl
    );
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG, 
            "error: could not initialize the repl. details: %s", 
            esp_err_to_name(error)
        );
        return 1;
    }

    error = esp_console_cmd_register(&set_gain_command_config);
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "error: could not register %s command. details: %s",
            set_gain_command_config.command,
            esp_err_to_name(error)
        );
        return 1;
    }

   error = esp_console_cmd_register(&read_voltage_command_config);
   if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "error: could not register %s command. details: %s",
            read_voltage_command_config.command,
            esp_err_to_name(error)
        );
   }

    error = esp_console_start_repl(repl);
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "error: could not start repl. details: %s",
            esp_err_to_name(error)
        );
        return 1;
    }

    return 0;

}

int cli_set_vga_gain(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "error: expecting 1 argument, %d passed instead", argc - 1);
        return 1;
    }

    long gain = atol(argv[1]);

    if (gain < 0 || gain > 64 || (gain & (gain - 1)) != 0) {
        fprintf(stderr, "error: gain %ld is not zero or a power of two less than or equal to 64", gain);
        return 1;
    }

    return set_vga_gain((unsigned int) gain);
}

int cli_voltage(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "error: expecting 1 argument, %d passed instead", argc - 1);
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