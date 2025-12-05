#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define I2C_DEVICE "/dev/i2c-0"
#define SSD1306_ADDR 0x3C
#define WIDTH 128
#define HEIGHT 64
#define BUFFER_SIZE (WIDTH * HEIGHT / 8)

int i2c_fd;
unsigned char framebuffer[BUFFER_SIZE];

void ssd1306_command(unsigned char cmd) {
    unsigned char buf[2];
    buf[0] = 0x00;
    buf[1] = cmd;
    write(i2c_fd, buf, 2);
}

void ssd1306_data(unsigned char data) {
    unsigned char buf[2];
    buf[0] = 0x40;
    buf[1] = data;
    write(i2c_fd, buf, 2);
}

void ssd1306_init() {
    ssd1306_command(0xAE);  // Display OFF
    usleep(100000);
    
    ssd1306_command(0xA8);  // Set Multiplex Ratio
    ssd1306_command(0x3F);
    
    ssd1306_command(0xD3);  // Set Display Offset
    ssd1306_command(0x00);
    
    ssd1306_command(0x40);  // Set Display Start Line
    
    ssd1306_command(0xA1);  // Set Segment Re-map
    ssd1306_command(0xC8);  // Set COM Output Scan Direction
    
    ssd1306_command(0xDA);  // Set COM Pins
    ssd1306_command(0x32);
    
    ssd1306_command(0xA4);  // Display RAM content
    ssd1306_command(0xA6);  // Normal display
    
    ssd1306_command(0xD5);  // Set Display Clock
    ssd1306_command(0x80);
    
    ssd1306_command(0x8D);  // Charge Pump
    ssd1306_command(0x14);
    
    ssd1306_command(0x20);  // Memory mode
    ssd1306_command(0x00);  // Horizontal addressing
    
    ssd1306_command(0xAF);  // Display ON
    usleep(1000);
}

void reset_cursor() {
    ssd1306_command(0x21);  // column address
    ssd1306_command(0x00);
    ssd1306_command(0x7F);
    ssd1306_command(0x22);  // page address
    ssd1306_command(0x00);
    ssd1306_command(0x07);
}

void clear_display() {
    reset_cursor();
    for (int i = 0; i < 1024; i++) {
        ssd1306_data(0x00);
    }
}

void clear_framebuffer() {
    memset(framebuffer, 0, BUFFER_SIZE);
}

void set_pixel(int x, int y, int value) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    
    int page = y / 8;
    int bit = y % 8;
    
    if (value) {
        framebuffer[page * WIDTH + x] |= (1 << bit);
    } else {
        framebuffer[page * WIDTH + x] &= ~(1 << bit);
    }
}

// КРУПНЫЙ ШРИФТ 8x8 для цифр и основных символов
void draw_big_char(int x, int y, char c) {
    // Простой крупный шрифт - только цифры и основные символы
    switch(c) {
        case '0':
            for (int i = 0; i < 8; i++) {
                set_pixel(x, y+i, 1); set_pixel(x+6, y+i, 1);
            }
            for (int i = 1; i < 6; i++) {
                set_pixel(x+i, y, 1); set_pixel(x+i, y+7, 1);
            }
            break;
        case '1':
            for (int i = 0; i < 8; i++) set_pixel(x+3, y+i, 1);
            set_pixel(x+2, y+1, 1); set_pixel(x+1, y+2, 1);
            break;
        case '2':
            for (int i = 0; i < 8; i++) {
                if (i < 4) set_pixel(x+6, y+i, 1);
                if (i > 3) set_pixel(x, y+i, 1);
            }
            for (int i = 0; i < 7; i++) set_pixel(x+i, y, 1);
            set_pixel(x+5, y+4, 1); set_pixel(x+4, y+4, 1);
            set_pixel(x+1, y+4, 1); set_pixel(x+2, y+4, 1);
            set_pixel(x+3, y+4, 1);
            for (int i = 0; i < 7; i++) set_pixel(x+i, y+7, 1);
            break;
        case '3':
            for (int i = 0; i < 8; i++) set_pixel(x+6, y+i, 1);
            for (int i = 0; i < 7; i++) {
                set_pixel(x+i, y, 1); 
                set_pixel(x+i, y+7, 1);
                set_pixel(x+i, y+3, 1);
            }
            break;
        case '4':
            for (int i = 0; i < 8; i++) set_pixel(x+6, y+i, 1);
            for (int i = 0; i < 4; i++) set_pixel(x, y+i, 1);
            for (int i = 0; i < 7; i++) set_pixel(x+i, y+4, 1);
            break;
        case '5':
            for (int i = 0; i < 4; i++) set_pixel(x, y+i, 1);
            for (int i = 4; i < 8; i++) set_pixel(x+6, y+i, 1);
            for (int i = 0; i < 7; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+3, 1);
                set_pixel(x+i, y+7, 1);
            }
            break;
        case '6':
            for (int i = 0; i < 8; i++) set_pixel(x, y+i, 1);
            for (int i = 4; i < 8; i++) set_pixel(x+6, y+i, 1);
            for (int i = 0; i < 7; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+3, 1);
                set_pixel(x+i, y+7, 1);
            }
            break;
        case '7':
            for (int i = 0; i < 8; i++) set_pixel(x+6, y+i, 1);
            for (int i = 0; i < 7; i++) set_pixel(x+i, y, 1);
            break;
        case '8':
            for (int i = 0; i < 8; i++) {
                set_pixel(x, y+i, 1); set_pixel(x+6, y+i, 1);
            }
            for (int i = 1; i < 6; i++) {
                set_pixel(x+i, y, 1); 
                set_pixel(x+i, y+3, 1);
                set_pixel(x+i, y+7, 1);
            }
            break;
        case '9':
            for (int i = 0; i < 4; i++) set_pixel(x, y+i, 1);
            for (int i = 0; i < 8; i++) set_pixel(x+6, y+i, 1);
            for (int i = 0; i < 7; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+3, 1);
                set_pixel(x+i, y+7, 1);
            }
            break;
        case '%':
            set_pixel(x+6, y, 1); set_pixel(x+5, y+1, 1); 
            set_pixel(x+4, y+2, 1); set_pixel(x+3, y+3, 1);
            set_pixel(x+2, y+4, 1); set_pixel(x+1, y+5, 1);
            set_pixel(x, y+6, 1); set_pixel(x, y+1, 1);
            set_pixel(x+1, y, 1); set_pixel(x+6, y+5, 1);
            set_pixel(x+5, y+6, 1); set_pixel(x+6, y+6, 1);
            break;
        case '.':
            set_pixel(x+1, y+6, 1); set_pixel(x+1, y+7, 1);
            set_pixel(x+2, y+6, 1); set_pixel(x+2, y+7, 1);
            break;
        case 'C':
            for (int i = 1; i < 7; i++) set_pixel(x, y+i, 1);
            for (int i = 1; i < 6; i++) {
                set_pixel(x+i, y, 1); set_pixel(x+i, y+7, 1);
            }
            break;
        case 'P':
            for (int i = 0; i < 8; i++) set_pixel(x, y+i, 1);
            for (int i = 1; i < 6; i++) {
                set_pixel(x+i, y, 1); set_pixel(x+i, y+3, 1);
            }
            set_pixel(x+6, y+1, 1); set_pixel(x+6, y+2, 1);
            break;
        case 'U':
            for (int i = 0; i < 7; i++) {
                set_pixel(x, y+i, 1); set_pixel(x+6, y+i, 1);
            }
            for (int i = 1; i < 6; i++) set_pixel(x+i, y+7, 1);
            break;
        case ' ': // пробел - ничего не рисуем
            break;
    }
}

void draw_big_string(int x, int y, const char *str) {
    int start_x = x;
    while (*str) {
        draw_big_char(start_x, y, *str);
        start_x += 8; // расстояние между символами
        str++;
    }
}

// ОЧЕНЬ КРУПНЫЕ ЦИФРЫ для процентов (16x24)
void draw_huge_number(int x, int y, int number) {
    // Упрощенные очень крупные цифры
    int digits[3];
    digits[0] = number / 100;
    digits[1] = (number % 100) / 10;
    digits[2] = number % 10;
    
    int start_x = x;
    for (int d = 0; d < 3; d++) {
        if (d == 0 && digits[0] == 0) continue; // не показывать ведущий ноль
        
        switch(digits[d]) {
            case 0:
                for (int i = 0; i < 24; i++) {
                    set_pixel(start_x, y+i, 1); set_pixel(start_x+14, y+i, 1);
                }
                for (int i = 1; i < 14; i++) {
                    set_pixel(start_x+i, y, 1); set_pixel(start_x+i, y+23, 1);
                }
                break;
            case 1:
                for (int i = 0; i < 24; i++) set_pixel(start_x+7, y+i, 1);
                set_pixel(start_x+6, y+1, 1); set_pixel(start_x+5, y+2, 1);
                set_pixel(start_x+4, y+3, 1);
                break;
            case 2:
                // Упрощенная двойка
                for (int i = 0; i < 8; i++) set_pixel(start_x+14, y+i, 1);
                for (int i = 8; i < 16; i++) set_pixel(start_x, y+i, 1);
                for (int i = 16; i < 24; i++) set_pixel(start_x+14, y+i, 1);
                for (int i = 0; i < 15; i++) {
                    set_pixel(start_x+i, y, 1);
                    set_pixel(start_x+i, y+7, 1);
                    set_pixel(start_x+i, y+15, 1);
                    set_pixel(start_x+i, y+23, 1);
                }
                break;
            case 9:
                // Упрощенная девятка
                for (int i = 0; i < 12; i++) set_pixel(start_x, y+i, 1);
                for (int i = 0; i < 24; i++) set_pixel(start_x+14, y+i, 1);
                for (int i = 0; i < 15; i++) {
                    set_pixel(start_x+i, y, 1);
                    set_pixel(start_x+i, y+11, 1);
                    set_pixel(start_x+i, y+23, 1);
                }
                break;
        }
        start_x += 18; // расстояние между крупными цифрами
    }
    
    // Знак процента после цифр
    set_pixel(start_x, y+2, 1); set_pixel(start_x+1, y+1, 1);
    set_pixel(start_x+2, y, 1); set_pixel(start_x+12, y+22, 1);
    set_pixel(start_x+13, y+23, 1); set_pixel(start_x+14, y+22, 1);
    set_pixel(start_x+3, y+4, 1); set_pixel(start_x+4, y+5, 1);
    set_pixel(start_x+5, y+6, 1); set_pixel(start_x+9, y+18, 1);
    set_pixel(start_x+10, y+19, 1); set_pixel(start_x+11, y+20, 1);
}

void draw_progress_bar(int x, int y, int width, int height, float percentage) {
    // Рамка
    for (int i = x; i < x + width; i++) {
        set_pixel(i, y, 1);
        set_pixel(i, y + height - 1, 1);
    }
    for (int i = y; i < y + height; i++) {
        set_pixel(x, i, 1);
        set_pixel(x + width - 1, i, 1);
    }
    
    // Заполнение
    int fill_width = (int)((percentage / 100.0) * (width - 2));
    for (int i = x + 1; i < x + 1 + fill_width; i++) {
        for (int j = y + 1; j < y + height - 1; j++) {
            set_pixel(i, j, 1);
        }
    }
}

void update_display() {
    reset_cursor();
    for (int i = 0; i < BUFFER_SIZE; i++) {
        ssd1306_data(framebuffer[i]);
    }
}

float get_cpu_usage() {
    static unsigned long long last_idle = 0, last_total = 0;
    FILE* file = fopen("/proc/stat", "r");
    if (!file) return 0.0;
    
    char buffer[256];
    fgets(buffer, sizeof(buffer), file);
    fclose(file);
    
    unsigned long long user, nice, system, idle, iowait, irq, softirq;
    sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu", 
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);
    
    unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
    unsigned long long total_diff = total - last_total;
    unsigned long long idle_diff = idle - last_idle;
    
    last_total = total;
    last_idle = idle;
    
    if (total_diff == 0) return 0.0;
    
    return 100.0 * (1.0 - ((float)idle_diff / total_diff));
}

int main() {
    // Открываем I2C устройство
    if ((i2c_fd = open(I2C_DEVICE, O_RDWR)) < 0) {
        perror("Failed to open I2C device");
        return 1;
    }
    
    // Устанавливаем адрес устройства
    if (ioctl(i2c_fd, I2C_SLAVE, SSD1306_ADDR) < 0) {
        perror("Failed to set I2C address");
        close(i2c_fd);
        return 1;
    }
    
    // Инициализируем дисплей
    ssd1306_init();
    clear_display();
    
    printf("Монитор CPU запущен. Нажмите Ctrl+C для выхода.\n");
    
    char text_buffer[32];
    while (1) {
        float cpu_usage = get_cpu_usage();
        int cpu_int = (int)(cpu_usage + 0.5); // округление
        
        // Очищаем фреймбуфер
        clear_framebuffer();
        
        // ВАРИАНТ 1: Очень крупные цифры процентов
        draw_huge_number(10, 10, cpu_int);
        
        // ВАРИАНТ 2: Крупный текст (раскомментировать если нужно)
        // snprintf(text_buffer, sizeof(text_buffer), "CPU %d%%", cpu_int);
        // draw_big_string(15, 5, text_buffer);
        
        // Прогресс-бар внизу
        draw_progress_bar(10, 50, 108, 10, cpu_usage);
        
        // Обновляем дисплей
        update_display();
        
        printf("CPU Usage: %.1f%%\n", cpu_usage);
        sleep(1);
    }
    
    close(i2c_fd);
    return 0;
}