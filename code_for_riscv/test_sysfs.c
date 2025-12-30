#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define GPIO_OFFSET 144
#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_DIR_TEMPLATE "/sys/class/gpio/gpio%d/direction"
#define GPIO_VALUE_TEMPLATE "/sys/class/gpio/gpio%d/value"

#define TARGET_INTERVAL_NS 10000000L  

volatile sig_atomic_t stop_flag = 0;

void signal_handler(int sig) {
    stop_flag = 1;
}

void export_gpio(int offset) {
    FILE *export_file = fopen(GPIO_EXPORT, "w");
    if (export_file) {
        fprintf(export_file, "%d", offset);
        fclose(export_file);
        usleep(100000);
    }
}

void unexport_gpio(int offset) {
    FILE *unexport_file = fopen(GPIO_UNEXPORT, "w");
    if (unexport_file) {
        fprintf(unexport_file, "%d", offset);
        fclose(unexport_file);
    }
}

void set_gpio_direction(int offset, const char *direction) {
    char path[100];
    snprintf(path, sizeof(path), GPIO_DIR_TEMPLATE, offset);
    
    FILE *dir_file = fopen(path, "w");
    if (dir_file) {
        fprintf(dir_file, "%s", direction);
        fclose(dir_file);
    }
}

void set_gpio_value(int offset, int value) {
    char path[100];
    snprintf(path, sizeof(path), GPIO_VALUE_TEMPLATE, offset);
    
    FILE *value_file = fopen(path, "w");
    if (value_file) {
        fprintf(value_file, "%d", value);
        fflush(value_file);
        fclose(value_file);
    }
}

int main() {
    // Установка обработчика сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Экспортируем и настраиваем GPIO
    export_gpio(GPIO_OFFSET);
    set_gpio_direction(GPIO_OFFSET, "out");
    
    // Убеждаемся, что GPIO выключен перед началом
    set_gpio_value(GPIO_OFFSET, 0);
    
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = TARGET_INTERVAL_NS;
    
    // Бесконечный цикл генерации импульсов
    while (!stop_flag) {
        // Устанавливаем GPIO в 1
        set_gpio_value(GPIO_OFFSET, 1);
        
        // Задержка включенного состояния
        nanosleep(&delay, NULL);
        
        // Устанавливаем GPIO в 0
        set_gpio_value(GPIO_OFFSET, 0);
        
        // Задержка выключенного состояния
        nanosleep(&delay, NULL);
    }
    
    // Гарантированно выключаем GPIO перед выходом
    set_gpio_value(GPIO_OFFSET, 0);
    unexport_gpio(GPIO_OFFSET);
    
    return 0;
}
