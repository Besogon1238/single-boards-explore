#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>

#define CHIP_NAME "/sysgpiochip0"
#define GPIO_LINE 4
#define GPIO_PLACE_IN_LINE 16
#define TARGET_INTERVAL_NS 10000000L

volatile sig_atomic_t stop_flag = 0;

void signal_handler(int sig) {
    stop_flag = 1;
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line_info *line;
    struct timespec delay = {0, TARGET_INTERVAL_NS};
    
    unsigned int gpio_offset = (GPIO_LINE*32)+GPIO_PLACE_IN_LINE;


    signal(SIGINT, signal_handler);
    
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("Open chip failed");
        return 1;
    }
    
    line = gpiod_chip_get_line_info(chip, gpio_offset);
    if (!line) {
        perror("Get line failed");
        gpiod_chip_close(chip);
        return 1;
    }
    
    struct gpiod_line_settings *settings;
    settings = gpiod_line_settings_new();
    if (!settings) {
        perror("Create settings failed");
        gpiod_chip_close(chip);
        return 1;
    }
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);
    
    struct gpiod_line_config *line_config;
    line_config = gpiod_line_config_new();
    if (!line_config) {
        perror("Create line config failed");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    
    int ret = gpiod_line_config_add_line_settings(line_config, &gpio_offset, 1, settings);
    if (ret < 0) {
        perror("Add line settings failed");
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }

    struct gpiod_request_config *req_config;
    req_config = gpiod_request_config_new();
    if (!req_config) {
        perror("Create request config failed");
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_request_config_set_consumer(req_config, "pulse-generator");

    struct gpiod_line_request *request = gpiod_chip_request_lines(chip, req_config, line_config);
    if (!request) {
        perror("Request lines failed");
        gpiod_request_config_free(req_config);
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    
    printf("Line requested successfully. Starting pulse generation...\n");

    while (!stop_flag) {
        gpiod_line_request_set_value(request,gpio_offset,1);
        nanosleep(&delay, NULL);
        gpiod_line_request_set_value(request,gpio_offset,0);
        nanosleep(&delay, NULL);
    }
    
    printf("Cleaning up...\n");
    gpiod_chip_close(chip);
    return 0;
}