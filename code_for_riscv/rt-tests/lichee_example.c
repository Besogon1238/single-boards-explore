#include <ctype.h>
#include <gpiod.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#define GPIO_CHIP      "/dev/gpiochip0"
#define GPIO_LINE 4
#define GPIO_IN_LINE   15
#define GPIO_OUT_LINE  12

#define RT_PRIORITY    99

static volatile int running = 1;

static void setup_rt(int pid)
{
    struct sched_param sp = {
        .sched_priority = RT_PRIORITY
    };

    if (sched_setscheduler(pid, SCHED_FIFO, &sp) < 0)
        perror("sched_setscheduler");

    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0)
        perror("mlockall");
}


static void sig_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void set_irq_thread_priority(int irq_number)
{
    char thread_name[64];
    char path[256];
    char pid_str[16];
    FILE *fp;
    DIR *dir;
    struct dirent *entry;
    
    snprintf(thread_name, sizeof(thread_name), "irq/%d-", irq_number);
    
    dir = opendir("/proc");
    if (!dir) {
        perror("opendir /proc");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;
        
        snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
        fp = fopen(path, "r");
        if (!fp) continue;

        char comm[64];
        if (fgets(comm, sizeof(comm), fp)) {
            comm[strcspn(comm, "\n")] = 0;
            
            if (strstr(comm, thread_name)) {
                pid_t pid = atoi(entry->d_name);
                
                setup_rt(pid);
            }
        }
        fclose(fp);
    }
    closedir(dir);
}

int main(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line_request *req;
    struct gpiod_line_settings *in_cfg, *out_cfg;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    unsigned int offsets[2];

    // Calculate offsets
    offsets[0] = (GPIO_LINE * 32) + GPIO_IN_LINE;   // Input
    offsets[1] = (GPIO_LINE * 32) + GPIO_OUT_LINE;  // Output

    //set_irq_thread_priority(132);

    setup_rt(0);

    signal(SIGINT, sig_handler);

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        return 1;
    }

    in_cfg = gpiod_line_settings_new();
    if (!in_cfg) {
        perror("Failed to create input settings");
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_line_settings_set_direction(in_cfg, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(in_cfg, GPIOD_LINE_EDGE_BOTH);

    out_cfg = gpiod_line_settings_new();
    if (!out_cfg) {
        perror("Failed to create output settings");
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_line_settings_set_direction(out_cfg, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(out_cfg, 0);

    line_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg, &offsets[0], 1, in_cfg);
    gpiod_line_config_add_line_settings(line_cfg, &offsets[1], 1, out_cfg);

    // Request configuration
    req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        perror("Failed to create request config");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(in_cfg);
        gpiod_line_settings_free(out_cfg);
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_request_config_set_consumer(req_cfg, "licheeeee");

    req = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    if (!req) {
        perror("gpiod_chip_request_lines");
        return 1;
    }

    struct gpiod_edge_event_buffer *evbuf;
    evbuf = gpiod_edge_event_buffer_new(1);

    int wait_status = 0;
    struct gpiod_edge_event *ev;

    while (running) {

        wait_status = gpiod_line_request_read_edge_events(req, evbuf,1);;

        ev = gpiod_edge_event_buffer_get_event(evbuf, 0);

        //printf("\nEvent!");

        if (gpiod_edge_event_get_event_type(ev) == GPIOD_EDGE_EVENT_RISING_EDGE) {
          
            gpiod_line_request_set_value(req, offsets[1], 1);
        
        }
        else
        {
           
            gpiod_line_request_set_value(req, offsets[1], 0);
        
        }
    }

    gpiod_edge_event_buffer_free(evbuf);
    gpiod_line_request_release(req);
    gpiod_chip_close(chip);
    return 0;
}