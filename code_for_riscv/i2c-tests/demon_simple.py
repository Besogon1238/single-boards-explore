#!/usr/bin/env python3
"""
Упрощенная версия с надежным получением CPU использования
"""

import time
import subprocess
from datetime import datetime
from luma.core.interface.serial import i2c
from luma.oled.device import ssd1306
from luma.core.render import canvas

class SimpleLogDisplay:
    def __init__(self):
        self.serial = i2c(port=0, address=0x3C)
        self.device = ssd1306(self.serial, width=128, height=64)
    
    def get_cpu_usage(self):
        """Надежное получение загрузки CPU через /proc/stat"""
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
    
    def get_kernel_logs(self):
        """Получение логов ядра"""
        try:
            result = subprocess.check_output(
                ["dmesg", "--level=err,warn", "-n", "4"],
                stderr=subprocess.DEVNULL,
                timeout=2
            ).decode('utf-8', errors='ignore')
            
            lines = result.strip().split('\n')[-4:]
            processed = []
            
            for line in lines:
                if line.strip():
                    # Берем только последнюю часть сообщения
                    if ']' in line:
                        msg = line.split(']')[-1].strip()[:22]
                    else:
                        msg = line.strip()[:22]
                    processed.append(msg)
            
            return processed if processed else ["No messages"]
        except:
            return ["Reading logs..."]
    
    def display_info(self):
        """Отображение информации"""
        cpu = self.get_cpu_usage()
        logs = self.get_kernel_logs()
        
        with canvas(self.device) as draw:
            # Верхняя строка
            draw.text((0, 0), "%s CPU:%.1f%%" % (datetime.now().strftime('%H:%M'), cpu), fill="white")
            
            # Разделитель
            draw.line((0, 12, 128, 12), fill="white")
            
            # Логи
            for i, log in enumerate(logs):
                draw.text((0, 14 + i * 12), log, fill="white")
    
    def run(self):
        try:
            while True:
                self.display_info()
                time.sleep(3)
        except KeyboardInterrupt:
            self.device.clear()

if __name__ == "__main__":
    SimpleLogDisplay().run()
