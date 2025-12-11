#!/usr/bin/env python3
"""
Программа для вывода системной информации и всех логов ядра на SSD1306 дисплей
"""

import time
import subprocess
from datetime import datetime
from luma.core.interface.serial import i2c
from luma.oled.device import ssd1306
from luma.core.render import canvas
from PIL import ImageFont

class SystemInfoDisplay:
    def __init__(self):
        # Инициализация дисплея
        self.serial = i2c(port=0, address=0x3C)
        self.device = ssd1306(self.serial, width=128, height=64)
        
        # Загрузка шрифтов
        try:
            self.font_small = ImageFont.truetype("DejaVuSans.ttf", 10)
            self.font_medium = ImageFont.truetype("DejaVuSans.ttf", 12)
        except:
            print("Шрифты не найдены, используем встроенные")
            self.font_small = None
            self.font_medium = None
    
    def get_cpu_usage(self):
        """Получение загрузки CPU"""
        try:
            with open('/proc/stat', 'r') as f:
                first_line = f.readline().strip()
            
            parts = first_line.split()
            if len(parts) >= 5:
                user = int(parts[1])
                nice = int(parts[2])
                system = int(parts[3])
                idle = int(parts[4])
                total = user + nice + system + idle
                usage = ((total - idle) / total) * 100
                return usage
            return 0.0
        except:
            return 0.0
    
    def wrap_text(self, text, max_length=24):
        """Разбивает текст на строки с переносами"""
        if len(text) <= max_length:
            return [text]
        
        lines = []
        words = text.split()
        current_line = ""
        
        for word in words:
            if len(current_line + " " + word) <= max_length:
                if current_line:
                    current_line += " " + word
                else:
                    current_line = word
            else:
                if current_line:
                    lines.append(current_line)
                current_line = word
        
        if current_line:
            lines.append(current_line)
        
        return lines
    
    def get_kernel_logs(self, max_lines=5):
        """Получение ВСЕХ логов ядра без фильтрации"""
        try:
            # Получаем все сообщения из dmesg без фильтров
            result = subprocess.check_output(
                ["dmesg"],  # Без параметров уровня - выводим всё
                stderr=subprocess.DEVNULL,
                timeout=2
            ).decode('utf-8', errors='ignore')
            
            all_lines = []
            raw_lines = result.strip().split('\n')[-8:]  # Берем последние 8 строк
            
            for line in raw_lines:
                if line.strip():
                    # Извлекаем сообщение (после временной метки)
                    if ']' in line:
                        message = line.split(']', 1)[1].strip()
                    else:
                        message = line.strip()
                    
                    # Разбиваем на строки с переносами
                    wrapped_lines = self.wrap_text(message)
                    all_lines.extend(wrapped_lines)
            
            return all_lines[-max_lines:] if all_lines else ["No kernel messages"]
            
        except subprocess.TimeoutExpired:
            return ["Logs timeout"]
        except Exception as e:
            return [f"Error: {str(e)[:20]}"]
    
    def display_system_info(self):
        """Отображение системной информации и логов"""
        cpu_usage = self.get_cpu_usage()
        
        with canvas(self.device) as draw:
            # Заголовок с временем и CPU в printf-стиле
            header_text = "Time: %s CPU: %.1f%%" % (datetime.now().strftime('%H:%M'), cpu_usage)
            draw.text((0, 0), header_text, font=self.font_medium, fill="white")
            
            # Разделительная линия
            draw.line((0, 14, 128, 14), fill="white", width=1)
            
            # Логи ядра - 5 строк с переносами
            logs = self.get_kernel_logs(5)
            
            # Выводим логи на оставшееся пространство
            for i, log in enumerate(logs):
                y_position = 16 + i * 10
                draw.text((0, y_position), log, font=self.font_small, fill="white")
    
    def run(self):
        """Основной цикл"""
        print("Запуск монитора системы на SSD1306...")
        print("Вывод ВСЕХ логов ядра без фильтрации")
        try:
            while True:
                self.display_system_info()
                time.sleep(3)
        except KeyboardInterrupt:
            print("\nОстановка...")
        finally:
            self.device.clear()

if __name__ == "__main__":
    display = SystemInfoDisplay()
    display.run()
