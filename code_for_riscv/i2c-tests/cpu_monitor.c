#include <stdio.h>
#include <stdlib.h>
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

/**
 * Отправка команды на дисплей
 * cmd - код команды
 */
void ssd1306_command(unsigned char cmd) {
    unsigned char buf[2];
    buf[0] = 0x00;  // Байт управления: 0x00 для команд
    buf[1] = cmd;   // Код команды
    write(i2c_fd, buf, 2);
}

/**
 * Отправка данных на дисплей
 * data - данные для отображения
 */
void ssd1306_data(unsigned char data) {
    unsigned char buf[2];
    buf[0] = 0x40;  // Байт управления: 0x40 для данных
    buf[1] = data;  // Данные пикселей
    write(i2c_fd, buf, 2);
}

/**
 * Инициализация дисплея SSD1306
 * Последовательность команд из рабочего bash-скрипта
 */
void ssd1306_init() {
    ssd1306_command(0xAE);  // Display OFF
    usleep(100000);
    
    ssd1306_command(0xA8);  // Set Multiplex Ratio
    ssd1306_command(0x3F);  // value (64-1)
    
    ssd1306_command(0xD3);  // Set Display Offset
    ssd1306_command(0x00);  // no vertical shift
    
    ssd1306_command(0x40);  // Set Display Start Line
    
    ssd1306_command(0xA1);  // Set Segment Re-map (reverse horizontal)
    ssd1306_command(0xC8);  // Set COM Output Scan Direction (reverse vertical)
    
    ssd1306_command(0xDA);  // Set COM Pins
    ssd1306_command(0x32);  // Alternative pin configuration
    
    ssd1306_command(0xA4);  // Display RAM content
    ssd1306_command(0xA6);  // Normal display (black on white)
    
    ssd1306_command(0xD5);  // Set Display Clock
    ssd1306_command(0x80);  // max frequency
    
    ssd1306_command(0x8D);  // Charge Pump
    ssd1306_command(0x14);  // enable charge pump
    
    ssd1306_command(0x20);  // Memory mode
    ssd1306_command(0x00);  // Horizontal addressing
    
    ssd1306_command(0xAF);  // Display ON
    usleep(1000);
}

/**
 * Сброс курсора в начало дисплея
 * Устанавливает адреса столбцов и страниц
 */
void reset_cursor() {
    ssd1306_command(0x21);  // set column address
    ssd1306_command(0x00);  // start column
    ssd1306_command(0x7F);  // end column (127)
    
    ssd1306_command(0x22);  // set page address
    ssd1306_command(0x00);  // start page
    ssd1306_command(0x07);  // end page (7)
}

/**
 * Очистка дисплея (заполнение черным)
 */
void clear_display() {
    reset_cursor();
    for (int i = 0; i < 1024; i++) {
        ssd1306_data(0x00);
    }
}

/* ========== ФУНКЦИИ РАБОТЫ С ФРЕЙМБУФЕРОМ ========== */

/**
 * Очистка фреймбуфера в памяти
 */
void clear_framebuffer() {
    memset(framebuffer, 0, BUFFER_SIZE);
}

/**
 * Установка пикселя в фреймбуфере
 * x, y - координаты пикселя
 * value - 1 (белый) или 0 (черный)
 */
void set_pixel(int x, int y, int value) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    
    int page = y / 8;      // Номер страницы (0-7)
    int bit = y % 8;       // Бит в байте (0-7)
    
    if (value) {
        framebuffer[page * WIDTH + x] |= (1 << bit);
    } else {
        framebuffer[page * WIDTH + x] &= ~(1 << bit);
    }
}

/**
 * Рисование линии в фреймбуфере
 * x1, y1 - начальная точка
 * x2, y2 - конечная точка
 */
void draw_line(int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        set_pixel(x1, y1, 1);
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/* ========== ФУНКЦИИ ОТОБРАЖЕНИЯ ГРАФИКИ ========== */

/**
 * Рисование очень крупной цифры (16x24 пикселя)
 * x, y - левый верхний угол
 * digit - цифра от 0 до 9
 */
void draw_huge_digit(int x, int y, int digit) {
    // Паттерны для очень крупных цифр 16x24
    switch(digit) {
        case 0:
            // Внешний прямоугольник
            for (int i = 0; i < 24; i++) {
                set_pixel(x, y+i, 1); 
                set_pixel(x+15, y+i, 1);
            }
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1); 
                set_pixel(x+i, y+23, 1);
            }
            break;
            
        case 1:
            // Вертикальная линия со скосом
            for (int i = 0; i < 24; i++) set_pixel(x+8, y+i, 1);
            for (int i = 1; i <= 4; i++) {
                set_pixel(x+8-i, y+i, 1);
                set_pixel(x+7, y+1, 1);
            }
            break;
            
        case 2:
            // Двойка с закруглением
            for (int i = 0; i < 8; i++) set_pixel(x+15, y+i, 1);
            for (int i = 8; i < 16; i++) set_pixel(x, y+i, 1);
            for (int i = 16; i < 24; i++) set_pixel(x+15, y+i, 1);
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+8, 1);
                set_pixel(x+i, y+23, 1);
            }
            break;
            
        case 3:
            // Тройка
            for (int i = 0; i < 24; i++) set_pixel(x+15, y+i, 1);
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+11, 1);
                set_pixel(x+i, y+23, 1);
            }
            break;
            
        case 4:
            // Четверка
            for (int i = 0; i < 12; i++) set_pixel(x, y+i, 1);
            for (int i = 0; i < 24; i++) set_pixel(x+15, y+i, 1);
            for (int i = 0; i < 16; i++) set_pixel(x+i, y+12, 1);
            break;
            
        case 5:
            // Пятерка
            for (int i = 0; i < 12; i++) set_pixel(x, y+i, 1);
            for (int i = 12; i < 24; i++) set_pixel(x+15, y+i, 1);
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+11, 1);
                set_pixel(x+i, y+23, 1);
            }
            break;
            
        case 6:
            // Шестерка
            for (int i = 0; i < 24; i++) set_pixel(x, y+i, 1);
            for (int i = 12; i < 24; i++) set_pixel(x+15, y+i, 1);
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+11, 1);
                set_pixel(x+i, y+23, 1);
            }
            break;
            
        case 7:
            // Семерка
            for (int i = 0; i < 24; i++) set_pixel(x+15, y+i, 1);
            for (int i = 0; i < 16; i++) set_pixel(x+i, y, 1);
            for (int i = 1; i < 6; i++) set_pixel(x+14-i, y+i, 1);
            break;
            
        case 8:
            // Восьмерка
            for (int i = 0; i < 24; i++) {
                set_pixel(x, y+i, 1); 
                set_pixel(x+15, y+i, 1);
            }
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+11, 1);
                set_pixel(x+i, y+23, 1);
            }
            break;
            
        case 9:
            // Девятка
            for (int i = 0; i < 12; i++) set_pixel(x, y+i, 1);
            for (int i = 0; i < 24; i++) set_pixel(x+15, y+i, 1);
            for (int i = 0; i < 16; i++) {
                set_pixel(x+i, y, 1);
                set_pixel(x+i, y+11, 1);
                set_pixel(x+i, y+23, 1);
            }
            break;
    }
}

/**
 * Рисование красивого знака процента (16x16 пикселей)
 * x, y - левый верхний угол
 */
void draw_percent_sign(int x, int y) {
    // Рисуем два круга (упрощенно) и наклонную линию
    
    // Левый верхний круг (упрощенный)
    set_pixel(x+2, y+2, 1); set_pixel(x+3, y+1, 1); set_pixel(x+4, y+1, 1);
    set_pixel(x+5, y+2, 1); set_pixel(x+5, y+3, 1); set_pixel(x+5, y+4, 1);
    set_pixel(x+4, y+5, 1); set_pixel(x+3, y+5, 1); set_pixel(x+2, y+4, 1);
    
    // Правый нижний круг (упрощенный)
    set_pixel(x+10, y+11, 1); set_pixel(x+11, y+10, 1); set_pixel(x+12, y+10, 1);
    set_pixel(x+13, y+11, 1); set_pixel(x+13, y+12, 1); set_pixel(x+13, y+13, 1);
    set_pixel(x+12, y+14, 1); set_pixel(x+11, y+14, 1); set_pixel(x+10, y+13, 1);
    
    // Наклонная линия от (7,1) до (9,15)
    draw_line(x+7, y+1, x+9, y+15);
}

/**
 * Отображение очень крупного числа с процентом
 * x, y - позиция для отображения
 * number - число от 0 до 100
 */
void draw_huge_percentage(int x, int y, int number) {
    int digits[3];
    digits[0] = number / 100;        // сотни
    digits[1] = (number % 100) / 10; // десятки
    digits[2] = number % 10;         // единицы
    
    int current_x = x;
    
    // Рисуем цифры (пропускаем ведущий ноль)
    if (digits[0] > 0) {
        draw_huge_digit(current_x, y, digits[0]);
        current_x += 18;
    }
    
    draw_huge_digit(current_x, y, digits[1]);
    current_x += 18;
    
    draw_huge_digit(current_x, y, digits[2]);
    current_x += 18;
    
    // Рисуем красивый знак процента
    //draw_percent_sign(current_x, y + 4);
}

/**
 * Рисование прогресс-бара
 * x, y - левый верхний угол
 * width, height - размеры
 * percentage - заполненность в процентах
 */
void draw_progress_bar(int x, int y, int width, int height, float percentage) {
    // Рамка прогресс-бара
    for (int i = x; i < x + width; i++) {
        set_pixel(i, y, 1);
        set_pixel(i, y + height - 1, 1);
    }
    for (int i = y; i < y + height; i++) {
        set_pixel(x, i, 1);
        set_pixel(x + width - 1, i, 1);
    }
    
    // Заполненная часть
    int fill_width = (int)((percentage / 100.0) * (width - 2));
    for (int i = x + 1; i < x + 1 + fill_width; i++) {
        for (int j = y + 1; j < y + height - 1; j++) {
            set_pixel(i, j, 1);
        }
    }
}

/**
 * Обновление дисплея из фреймбуфера
 */
void update_display() {
    reset_cursor();
    for (int i = 0; i < BUFFER_SIZE; i++) {
        ssd1306_data(framebuffer[i]);
    }
}

/* ========== ФУНКЦИИ СИСТЕМНОГО МОНИТОРИНГА ========== */

/**
 * Получение загрузки CPU из /proc/stat
 * Возвращает загрузку в процентах
 */
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
    
    // Общее время работы CPU
    unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
    unsigned long long total_diff = total - last_total;
    unsigned long long idle_diff = idle - last_idle;
    
    // Сохраняем текущие значения для следующего вызова
    last_total = total;
    last_idle = idle;
    
    if (total_diff == 0) return 0.0;
    
    // Расчет загрузки: (1 - idle_time/total_time) * 100%
    return 100.0 * (1.0 - ((float)idle_diff / total_diff));
}

/* ========== ОСНОВНАЯ ПРОГРАММА ========== */

int main() {
    printf("Инициализация монитора CPU...\n");
    
    // 1. ОТКРЫТИЕ I2C УСТРОЙСТВА
    if ((i2c_fd = open(I2C_DEVICE, O_RDWR)) < 0) {
        perror("Ошибка открытия I2C устройства");
        return 1;
    }
    
    // 2. УСТАНОВКА АДРЕСА SSD1306
    if (ioctl(i2c_fd, I2C_SLAVE, SSD1306_ADDR) < 0) {
        perror("Ошибка установки I2C адреса");
        close(i2c_fd);
        return 1;
    }
    
    // 3. ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
    ssd1306_init();
    clear_display();
    
    printf("Монитор CPU запущен. Нажмите Ctrl+C для выхода.\n");
    
    // 4. ОСНОВНОЙ ЦИКЛ
    while (1) {
        // Получение загрузки CPU
        float cpu_usage = get_cpu_usage();
        int cpu_int = (int)(cpu_usage + 0.5); // округление до целого
        
        // Очистка фреймбуфера
        clear_framebuffer();
        
        // ОТОБРАЖЕНИЕ НА ДИСПЛЕЕ:
        // - Очень крупные цифры процента загрузки (центр экрана)
        draw_huge_percentage(10, 10, cpu_int);
        
        // - Прогресс-бар (внизу экрана)
        draw_progress_bar(10, 50, 108, 10, cpu_usage);
        
        // Обновление дисплея
        update_display();
        
        // Вывод в консоль для отладки
        printf("CPU Usage: %.1f%%\n", cpu_usage);
        
        // Пауза 1 секунда
        sleep(1);
    }
    
    // Закрытие I2C устройства (недостижимо в бесконечном цикле)
    close(i2c_fd);
    return 0;
}
