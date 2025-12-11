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

int i2c_fd;

// Команды SSD1306
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

void ssd1306_command(unsigned char cmd) {
    unsigned char buf[2];
    buf[0] = 0x00; // Control byte: Co=0, D/C#=0
    buf[1] = cmd;
    write(i2c_fd, buf, 2);
}

void ssd1306_init() {
    ssd1306_command(SSD1306_DISPLAYOFF);
    ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);
    ssd1306_command(0x80);
    ssd1306_command(SSD1306_SETMULTIPLEX);
    ssd1306_command(HEIGHT - 1);
    ssd1306_command(SSD1306_SETDISPLAYOFFSET);
    ssd1306_command(0x00);
    ssd1306_command(SSD1306_SETSTARTLINE | 0x00);
    ssd1306_command(SSD1306_CHARGEPUMP);
    ssd1306_command(0x14);
    ssd1306_command(SSD1306_MEMORYMODE);
    ssd1306_command(0x00);
    ssd1306_command(SSD1306_SEGREMAP | 0x1);
    ssd1306_command(SSD1306_COMSCANDEC);
    ssd1306_command(SSD1306_SETCOMPINS);
    ssd1306_command(0x12);
    ssd1306_command(SSD1306_SETCONTRAST);
    ssd1306_command(0xCF);
    ssd1306_command(SSD1306_SETPRECHARGE);
    ssd1306_command(0xF1);
    ssd1306_command(SSD1306_SETVCOMDETECT);
    ssd1306_command(0x40);
    ssd1306_command(SSD1306_DISPLAYALLON_RESUME);
    ssd1306_command(SSD1306_NORMALDISPLAY);
    ssd1306_command(SSD1306_DISPLAYON);
}

void ssd1306_clear() {
    unsigned char buf[WIDTH + 1];
    
    ssd1306_command(SSD1306_COLUMNADDR);
    ssd1306_command(0);
    ssd1306_command(WIDTH - 1);
    ssd1306_command(SSD1306_PAGEADDR);
    ssd1306_command(0);
    ssd1306_command(7);
    
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x40; // Control byte: Co=0, D/C#=1
    
    for (int page = 0; page < 8; page++) {
        write(i2c_fd, buf, WIDTH + 1);
    }
}

void ssd1306_draw_text(int x, int y, const char* text) {
    // Простой вывод текста (базовые символы)
    unsigned char buf[WIDTH + 1];
    buf[0] = 0x40;
    
    // Простая реализация для демонстрации
    // В реальном приложении лучше использовать шрифт
}

void draw_progress_bar(int x, int y, int width, int height, int percentage) {
    // Упрощенная реализация прогресс-бара
    // В реальном приложении нужно реализовать полноценную графику
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
    ssd1306_clear();
    
    printf("CPU Monitor started. Press Ctrl+C to exit.\n");
    
    while (1) {
        float cpu_usage = get_cpu_usage();
        
        // Здесь должна быть реализация отображения на дисплее
        // Для простоты просто очищаем и выводим базовую информацию
        ssd1306_clear();
        
        // В реальной реализации здесь был бы код для отрисовки
        // текста и прогресс-бара
        
        printf("CPU Usage: %.1f%%\n", cpu_usage);
        sleep(1);
    }
    
    close(i2c_fd);
    return 0;
}