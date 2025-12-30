#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>
#include <signal.h>

#define CHIP_PATH "/dev/gpiochip0"
#define GPIO_LINE 4
#define GPIO_INPUT_PLACE_IN_LINE 16   // Input pin (GPIO 140)
#define GPIO_OUTPUT_PLACE_IN_LINE 12  // Output pin (GPIO 144)

#define PULSE_NS 1000L  // Small nanosleep to avoid CPU overuse

volatile sig_atomic_t stop_flag = 0;

void signal_handler(int sig)
{
    stop_flag = 1;
}

int main()
{
    struct gpiod_chip *chip;
    unsigned int offsets[2];
    struct gpiod_line_settings *in_settings, *out_settings;
    struct gpiod_line_config *line_config;
    struct gpiod_request_config *req_config;
    struct gpiod_line_request *request;

    struct timespec last_rise_time = {0, 0};
    struct timespec current_time = {0, 0};
    long interval_ns = 0;
    int first_pulse = 1;

    signal(SIGINT, signal_handler);

    // Calculate offsets
    offsets[0] = (GPIO_LINE * 32) + GPIO_INPUT_PLACE_IN_LINE;   // Input
    offsets[1] = (GPIO_LINE * 32) + GPIO_OUTPUT_PLACE_IN_LINE;  // Output

    printf("Opening GPIO chip...\n");
    chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        perror("gpiod_chip_open failed");
        return 1;
    }

    // Configure input pin
    in_settings = gpiod_line_settings_new();
    if (!in_settings) {
        perror("Failed to create input settings");
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_line_settings_set_direction(in_settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(in_settings, GPIOD_LINE_EDGE_RISING);

    // Configure output pin
    out_settings = gpiod_line_settings_new();
    if (!out_settings) {
        perror("Failed to create output settings");
        gpiod_line_settings_free(in_settings);
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_line_settings_set_direction(out_settings, GPIOD_LINE_DIRECTION_OUTPUT);

    // Create line configuration
    line_config = gpiod_line_config_new();
    if (!line_config) {
        perror("Failed to create line config");
        gpiod_line_settings_free(in_settings);
        gpiod_line_settings_free(out_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    // Add settings for each line
    if (gpiod_line_config_add_line_settings(line_config, &offsets[0], 1, in_settings) < 0 ||
        gpiod_line_config_add_line_settings(line_config, &offsets[1], 1, out_settings) < 0) {
        perror("Failed to add line settings");
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(in_settings);
        gpiod_line_settings_free(out_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    // Request configuration
    req_config = gpiod_request_config_new();
    if (!req_config) {
        perror("Failed to create request config");
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(in_settings);
        gpiod_line_settings_free(out_settings);
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_request_config_set_consumer(req_config, "lichee-response");

    // Request lines
    request = gpiod_chip_request_lines(chip, req_config, line_config);
    if (!request) {
        perror("Failed to request lines");
        gpiod_request_config_free(req_config);
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(in_settings);
        gpiod_line_settings_free(out_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    printf("Running. Waiting for input on GPIO %u...\n", offsets[0]);
    printf("Output on GPIO %u\n", offsets[1]);

    int last_value = 0;
    int wait_status = 0;

    while (!stop_flag) {
        wait_status = gpiod_line_request_wait_edge_events(request, -1);

        // Read input pin value
        int value = gpiod_line_request_get_value(request, offsets[0]);

        if (value < 0) {
            perror("Failed to read input");
            break;
        }

        // Detect rising edge (0 -> 1)
        if (last_value == 0 && value == 1) {
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            if (!first_pulse) {
                interval_ns = (current_time.tv_sec - last_rise_time.tv_sec) * 1000000000L;
                interval_ns += (current_time.tv_nsec - last_rise_time.tv_nsec);
                // printf("Rising edge detected! Interval: %ld ns \n", interval_ns);
            } else {
                // printf("First rising edge detected \n");
                first_pulse = 0;
            }

            last_rise_time = current_time;
            gpiod_line_request_set_value(request, offsets[1], 1);
        } else if (last_value == 1 && value == 0) {
            // printf("Falling edge detected! Stop pulse...\n");
            gpiod_line_request_set_value(request, offsets[1], 0);
        }

        last_value = value;
    }

    // Cleanup
    gpiod_request_config_free(req_config);
    gpiod_line_config_free(line_config);
    gpiod_line_settings_free(in_settings);
    gpiod_line_settings_free(out_settings);
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);

    printf("Exiting...\n");
    return 0;
}