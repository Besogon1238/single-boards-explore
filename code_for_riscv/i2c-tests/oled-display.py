#!/usr/bin/env python3

import time
import subprocess
import sys
from datetime import datetime
from pathlib import Path

try:
    from luma.core.interface.serial import i2c
    from luma.oled.device import ssd1306
    from luma.core.render import canvas
except ImportError as e:
    print(f"Error: {e}")
    sys.exit(1)

class OLEDDisplay:
    def __init__(self):
        print("Starting OLED display...")
        try:
            self.serial = i2c(port=0, address=0x3C)
            self.device = ssd1306(self.serial, width=128, height=64)
            print("Display initialized")
        except Exception as e:
            print(f"Display error: {e}")
            sys.exit(1)
    
    def get_cpu_usage(self):
        try:
            with open('/proc/stat', 'r') as f:
                first_line = f.readline().strip()
            parts = first_line.split()
            user = int(parts[1])
            nice = int(parts[2])
            system = int(parts[3])
            idle = int(parts[4])
            total = user + nice + system + idle
            return ((total - idle) / total) * 100
        except:
            return 0.0
    
    def get_kernel_logs(self, lines=5):
        try:
            result = subprocess.check_output(
                ["dmesg"],
                timeout=2
            ).decode('utf-8', errors='ignore')
            
            all_logs = []
            raw_lines = result.strip().split('\n')[-lines*2:]
            
            for line in raw_lines:
                if line.strip():
                    if ']' in line:
                        msg = line.split(']', 1)[1].strip()
                    else:
                        msg = line.strip()
                    
                    if len(msg) > 24:
                        msg = msg[:24]
                    all_logs.append(msg)
            
            return all_logs[-lines:] if all_logs else ["No messages"]
        except:
            return ["Reading logs..."]
    
    def display_info(self):
        cpu = self.get_cpu_usage()
        logs = self.get_kernel_logs(5)
        
        with canvas(self.device) as draw:
            draw.text((0, 0), f"{datetime.now().strftime('%H:%M')} CPU:{cpu:.1f}%", fill="white")
            draw.line((0, 12, 128, 12), fill="white")
            
            for i, log in enumerate(logs):
                y_pos = 14 + i * 10
                draw.text((0, y_pos), log, fill="white")
    
    def run(self):
        print("OLED monitor running...")
        try:
            while True:
                self.display_info()
                time.sleep(3)
        except KeyboardInterrupt:
            print("Stopping...")
        finally:
            self.device.clear()

if __name__ == "__main__":
    # Check I2C
    if not Path('/dev/i2c-0').exists():
        print("I2C device not found")
        sys.exit(1)
    
    display = OLEDDisplay()
    display.run()
