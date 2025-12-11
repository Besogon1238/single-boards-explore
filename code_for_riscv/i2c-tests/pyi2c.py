#!/usr/bin/env python3

import time
import psutil
from luma.core.interface.serial import i2c
from luma.oled.device import ssd1306
from luma.core.render import canvas

def get_system_info():
    cpu = psutil.cpu_percent()
    memory = psutil.virtual_memory()
    disk = psutil.disk_usage('/')
    return {
        'cpu': f"CPU: {cpu}%",
        'mem': f"MEM: {memory.percent}%",
        'disk': f"DISK: {disk.percent}%"
    }

def main():
    serial = i2c(port=0, address=0x3C)
    device = ssd1306(serial)
    
    while True:
        info = get_system_info()
        with canvas(device) as draw:
            draw.text((0, 0), "System Info:", fill="white")
            draw.text((0, 15), info['cpu'], fill="white")
            draw.text((0, 30), info['mem'], fill="white")
            draw.text((0, 45), info['disk'], fill="white")
        time.sleep(2)

if __name__ == "__main__":
    main()
