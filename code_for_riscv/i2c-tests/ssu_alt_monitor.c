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

/* ========== ФУНКЦИИ РАБОТЫ С I2C И ДИСПЛЕЕМ ========== */

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
    ssd1306_command(0xAE);
    usleep(100000);
    
    ssd1306_command(0xA8);
    ssd1306_command(0x3F);
    
    ssd1306_command(0xD3);
    ssd1306_command(0x00);
    
    ssd1306_command(0x40);
    
    ssd1306_command(0xA1);
    ssd1306_command(0xC8);
    
    ssd1306_command(0xDA);
    ssd1306_command(0x32);
    
    ssd1306_command(0xA4);
    ssd1306_command(0xA6);
    
    ssd1306_command(0xD5);
    ssd1306_command(0x80);
    
    ssd1306_command(0x8D);
    ssd1306_command(0x14);
    
    ssd1306_command(0x20);
    ssd1306_command(0x00);
    
    ssd1306_command(0xAF);
    usleep(1000);
}

void reset_cursor() {
    ssd1306_command(0x21);
    ssd1306_command(0x00);
    ssd1306_command(0x7F);
    ssd1306_command(0x22);
    ssd1306_command(0x00);
    ssd1306_command(0x07);
}

void clear_display() {
    reset_cursor();
    for (int i = 0; i < 1024; i++) {
        ssd1306_data(0x00);
    }
}

/* ========== ФУНКЦИИ РАБОТЫ С ФРЕЙМБУФЕРОМ ========== */

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

/* ========== БОЛЬШИЕ БУКВЫ 8x8 ========== */

void draw_big_char(int x, int y, char c) {
    switch(c) {
        case 'S':
            // Буква S
            for (int i = 1; i < 6; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+3, 1);
                set_pixel(x+i, y+7, 1);
            }
            set_pixel(x, y+1, 1); set_pixel(x, y+2, 1);
            set_pixel(x+6, y+4, 1); set_pixel(x+6, y+5, 1); set_pixel(x+6, y+6, 1);
            set_pixel(x+1, y+4, 0); set_pixel(x+1, y+5, 0); // небольшие вырезы для читаемости
            break;
            
        case 'U':
            // Буква U
            for (int i = 0; i < 7; i++) {
                set_pixel(x, y+i, 1);
                set_pixel(x+6, y+i, 1);
            }
            for (int i = 1; i < 6; i++) {
                set_pixel(x+i, y+7, 1);
            }
            break;
            
        case 'A':
            // Буква A
            for (int i = 2; i < 6; i++) {
                set_pixel(x+i, y, 1);
            }
            for (int i = 1; i < 7; i++) {
                set_pixel(x, y+i, 1);
                set_pixel(x+6, y+i, 1);
            }
            for (int i = 1; i < 6; i++) {
                set_pixel(x+i, y+3, 1);
            }
            break;
            
        case 'L':
            // Буква L
            for (int i = 0; i < 8; i++) {
                set_pixel(x, y+i, 1);
            }
            for (int i = 0; i < 7; i++) {
                set_pixel(x+i, y+7, 1);
            }
            break;
            
        case 'T':
            // Буква T
            for (int i = 0; i < 7; i++) {
                set_pixel(x+i, y, 1);
            }
            for (int i = 0; i < 8; i++) {
                set_pixel(x+3, y+i, 1);
            }
            break;
            
        case ' ':
            // Пробел - ничего не рисуем
            break;
    }
}

/* ========== ОЧЕНЬ БОЛЬШИЕ БУКВЫ 16x16 ========== */

void draw_huge_char(int x, int y, char c) {
    switch(c) {
        case 'S':
            // Очень большая S
            for (int i = 2; i < 14; i++) {
                set_pixel(x+i, y, 1);        // Верхняя горизонталь
                set_pixel(x+i, y+7, 1);      // Средняя горизонталь
                set_pixel(x+i, y+15, 1);     // Нижняя горизонталь
            }
            for (int i = 1; i < 7; i++) {
                set_pixel(x, y+i, 1);        // Левая верхняя вертикаль
                set_pixel(x+15, y+8+i, 1);   // Правая нижняя вертикаль
            }
            for (int i = 1; i < 2; i++) {
                set_pixel(x+1, y+i, 1);
                set_pixel(x+14, y+14, 1);
            }
            break;
            
        case 'U':
            // Очень большая U
            for (int i = 0; i < 15; i++) {
                set_pixel(x, y+i, 1);        // Левая вертикаль
                set_pixel(x+15, y+i, 1);     // Правая вертикаль
            }
            for (int i = 1; i < 15; i++) {
                set_pixel(x+i, y+15, 1);     // Нижняя горизонталь
            }
            for (int i = 1; i < 3; i++) {
                set_pixel(x+i, y+14, 1);
                set_pixel(x+14-i, y+14, 1);
            }
            break;
            
        case 'A':
            // Очень большая A
            for (int i = 4; i < 12; i++) {
                set_pixel(x+i, y, 1);        // Верхняя горизонталь
            }
            for (int i = 1; i < 15; i++) {
                set_pixel(x, y+i, 1);        // Левая наклонная
                set_pixel(x+15, y+i, 1);     // Правая наклонная
            }
            for (int i = 4; i < 12; i++) {
                set_pixel(x+i, y+7, 1);      // Средняя горизонталь
            }
            // Заполнение для лучшей читаемости
            for (int i = 1; i < 4; i++) {
                set_pixel(x+i, y+i, 1);
                set_pixel(x+15-i, y+i, 1);
            }
            break;
            
        case 'L':
            // Очень большая L
            for (int i = 0; i < 16; i++) {
                set_pixel(x, y+i, 1);        // Левая вертикаль
            }
            for (int i = 1; i < 16; i++) {
                set_pixel(x+i, y+15, 1);     // Нижняя горизонталь
            }
            // Утолщение
            set_pixel(x+1, y+14, 1);
            set_pixel(x+1, y+13, 1);
            break;
            
        case 'T':
            // Очень большая T
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1);        // Верхняя горизонталь
            }
            for (int i = 1; i < 16; i++) {
                set_pixel(x+7, y+i, 1);      // Вертикаль
                set_pixel(x+8, y+i, 1);      // Утолщение
            }
            break;
            
        case ' ':
            // Пробел
            break;
    }
}

/* ========== ФУНКЦИИ ВЫВОДА ТЕКСТА ========== */

void draw_big_text(int x, int y, const char *text) {
    int start_x = x;
    while (*text) {
        draw_big_char(start_x, y, *text);
        start_x += 8; // расстояние между символами
        text++;
    }
}

void draw_huge_text(int x, int y, const char *text) {
    int start_x = x;
    while (*text) {
        draw_huge_char(start_x, y, *text);
        start_x += 18; // расстояние между крупными символами
        text++;
    }
}

void update_display() {
    reset_cursor();
    for (int i = 0; i < BUFFER_SIZE; i++) {
        ssd1306_data(framebuffer[i]);
    }
}

/* ========== ФУНКЦИИ ВЫВОДА НАДПИСЕЙ ========== */

void display_ssu_alt_big() {
    clear_framebuffer();
    
    // Вычисляем центрирование для текста "SSU ALT"
    // Длина текста: 7 символов × 8px = 56px, пробелы добавляют ширину
    int text_width = 7 * 8;
    int start_x = (WIDTH - text_width) / 2;
    
    // Первая строка: "SSU"
    draw_big_text(start_x, 15, "SSU");
    
    // Вторая строка: "ALT"  
    draw_big_text(start_x + 4, 30, "ALT");
    
    update_display();
}

void display_ssu_alt_huge() {
    clear_framebuffer();
    
    // Вычисляем центрирование для очень крупного текста "SSU"
    int ssu_width = 3 * 18; // 3 символа × 18px
    int ssu_x = (WIDTH - ssu_width) / 2;
    
    // Вычисляем центрирование для очень крупного текста "ALT"
    int alt_width = 3 * 18; // 3 символа × 18px
    int alt_x = (WIDTH - alt_width) / 2;
    
    // Первая строка: очень крупная "SSU"
    draw_huge_text(ssu_x, 10, "SSU");
    
    // Вторая строка: очень крупная "ALT"
    draw_huge_text(alt_x, 33, "ALT");
    
    update_display();
}

void display_ssu_alt_single_line() {
    clear_framebuffer();
    
    // Одна строка: "SSU ALT" очень крупными буквами
    // Длина: 6 символов × 18px = 108px (пробел занимает место)
    int text_width = 6 * 18;
    int start_x = (WIDTH - text_width) / 2;
    
    draw_huge_text(start_x-4, 20, "SSU ALT");
    
    update_display();
}

/* ========== ОСНОВНАЯ ПРОГРАММА ========== */

int main() {
    printf("Инициализация дисплея для вывода SSU ALT...\n");
    
    // Открываем I2C устройство
    if ((i2c_fd = open(I2C_DEVICE, O_RDWR)) < 0) {
        perror("Ошибка открытия I2C устройства");
        return 1;
    }
    
    // Устанавливаем адрес SSD1306
    if (ioctl(i2c_fd, I2C_SLAVE, SSD1306_ADDR) < 0) {
        perror("Ошибка установки I2C адреса");
        close(i2c_fd);
        return 1;
    }
    
    // Инициализируем дисплей
    ssd1306_init();
    clear_display();
    
    printf("Дисплей инициализирован. Вывод SSU ALT...\n");
    
    // Вариант 1: Большие буквы в две строки
    printf("Вариант 1: Большие буквы (8x8) в две строки\n");
    display_ssu_alt_big();
    sleep(3);
    
    // Вариант 2: Очень большие буквы в две строки
    printf("Вариант 2: Очень большие буквы (16x16) в две строки\n");
    display_ssu_alt_huge();
    sleep(3);
    
    // Вариант 3: Очень большие буквы в одну строку
    printf("Вариант 3: Очень большие буквы (16x16) в одну строку\n");
    display_ssu_alt_single_line();
    
    printf("Надпись выведена. Программа завершает работу.\n");
    printf("Для выхода нажмите Ctrl+C\n");
    
    // Бесконечный цикл чтобы надпись оставалась на экране
    while (1) {
        sleep(1);
    }
    
    close(i2c_fd);
    return 0;
}
