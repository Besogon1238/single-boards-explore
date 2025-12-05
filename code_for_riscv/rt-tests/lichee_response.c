#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>

#define CHIP_PATH "/dev/gpiochip0"

#define GPIO_LINE 4
#define GPIO_INPUT_PLACE_IN_LINE 11
#define GPIO_OUTPUT_PLACE_IN_LINE 16

// Длительность импульса ответа (нс)
#define PULSE_NS 1000     // 1 мкс

// Медленное опрашивание — Lichee умеет быстро, но не надо грузить CPU
#define POLL_DELAY_US 50  // 50 мкс между чтениями входа

int main() 
{
    struct gpiod_chip *chip;
    struct gpiod_line_info *in_line, *out_line;

    unsigned int gpio_in_offset = (GPIO_LINE*32)+GPIO_INPUT_PLACE_IN_LINE;
    unsigned int gpio_out_offset = (GPIO_LINE*32)+GPIO_OUTPUT_PLACE_IN_LINE;


    printf("Opening GPIO chip...\n");
    chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        perror("gpiod_chip_open failed");
        return 1;
    }

    // Входной пин

    in_line = gpiod_chip_get_line_info(chip, gpio_in_offset);
    if (!in_line) {
        perror("gpiod_chip_get_line (input)");
        return 1;
    }

    struct gpiod_line_settings *input_line_settings;
    input_line_settings = gpiod_line_settings_new();
    if (!input_line_settings) {
        perror("Create settings failed");
        gpiod_chip_close(chip);
        return 1;
    }
    
    gpiod_line_settings_set_direction(input_line_settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_active_low(input_line_settings,0);
    gpiod_line_settings_set_edge_detection(input_line_settings,GPIOD_LINE_EDGE_RISING);
    gpiod_line_settings_set_bias(input_line_settings,GPIOD_LINE_BIAS_PULL_UP);

    struct gpiod_line_config *input_line_config;
    input_line_config = gpiod_line_config_new();
    if (!input_line_config) {
        perror("Create line config failed");
        gpiod_line_settings_free(input_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }
    
    int ret_input = gpiod_line_config_add_line_settings(input_line_config, &gpio_in_offset, 1, input_line_settings);
    if (ret_input < 0) {
        perror("Add line settings failed");
        gpiod_line_config_free(input_line_config);
        gpiod_line_settings_free(input_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    struct gpiod_request_config *req_input_config;
    req_input_config = gpiod_request_config_new();
    if (!req_input_config) {
        perror("Create request config failed");
        gpiod_line_config_free(input_line_config);
        gpiod_line_settings_free(input_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_request_config_set_consumer(req_input_config, "lichee-input");

    struct gpiod_line_request *request_input = gpiod_chip_request_lines(chip, req_input_config, input_line_config);
    if (!request_input) {
        perror("Request lines failed");
        gpiod_request_config_free(req_input_config);
        gpiod_line_config_free(input_line_config);
        gpiod_line_settings_free(input_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    // Выходной пин

    out_line = gpiod_chip_get_line_info(chip, gpio_out_offset);
    if (!out_line) {
        perror("Get line failed");
        gpiod_chip_close(chip);
        return 1;
    }
    
    struct gpiod_line_settings *output_line_settings;
    output_line_settings = gpiod_line_settings_new();
    if (!output_line_settings) {
        perror("Create settings failed");
        gpiod_chip_close(chip);
        return 1;
    }
    
    gpiod_line_settings_set_direction(output_line_settings, GPIOD_LINE_DIRECTION_AS_IS);
    gpiod_line_settings_set_output_value(output_line_settings, GPIOD_LINE_VALUE_INACTIVE);
    
    struct gpiod_line_config *output_line_config;
    output_line_config = gpiod_line_config_new();
    if (!output_line_config) {
        perror("Create line config failed");
        gpiod_line_settings_free(output_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }
    
    int ret = gpiod_line_config_add_line_settings(output_line_config, &gpio_out_offset, 1, output_line_settings);
    if (ret < 0) {
        perror("Add line settings failed");
        gpiod_line_config_free(output_line_config);
        gpiod_line_settings_free(output_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    struct gpiod_request_config *req_output_config;
    req_output_config = gpiod_request_config_new();
    if (!req_output_config) {
        perror("Create request config failed");
        gpiod_line_config_free(output_line_config);
        gpiod_line_settings_free(output_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_request_config_set_consumer(req_output_config, "lichee-output");

    struct gpiod_line_request *request_output = gpiod_chip_request_lines(chip, req_output_config, output_line_config);
    if (!request_output) {
        perror("Request lines failed");
        gpiod_request_config_free(req_output_config);
        gpiod_line_config_free(output_line_config);
        gpiod_line_settings_free(output_line_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    printf("Running. Waiting for input on GPIO %d...\n", gpio_in_offset);

    struct timespec pulse_time;
    pulse_time.tv_sec  = 0;
    pulse_time.tv_nsec = PULSE_NS;

    while (1) 
    {
        int value = gpiod_line_request_get_value(request_input,gpio_in_offset);

        if (value < 0) {
            perror("gpiod_line_get_value failed");
            break;
        }

        if (value == 0) {
            printf("Without info...\n");
        }

        if (value == 1) {
            // Нашли фронт
            // Генерируем импульс

            printf("Generate impulse \n");
            printf("Waiting... \n");

            // Ждём, пока Arduino опустит вход обратно в 0
            while (gpiod_line_request_get_value(request_input,gpio_in_offset) == 1) {
                usleep(POLL_DELAY_US);
            }
        }

        usleep(POLL_DELAY_US);
    }


    gpiod_chip_close(chip);

    return 0;
}
