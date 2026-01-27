import serial
import json
import time
import pandas as pd
from datetime import datetime
import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import os

mpl.use("agg")

class ArduinoDataReceiver:
    def __init__(self, port=None, baudrate=115200):
        """Инициализация соединения с Arduino"""
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.data = []
        self.session_id = None
        
    def auto_detect_port(self):
        """Автоматическое определение порта Arduino"""
        import serial.tools.list_ports
        
        ports = list(serial.tools.list_ports.comports())
        for port in ports:
            # Ищем Arduino Mega (обычно содержит 'Arduino' или 'USB')
            if 'Arduino' in port.description or 'ACM' in port.description:
                print(f"Найден Arduino на порту: {port.device}")
                return port.device
        
        # Если не нашли, показываем доступные порты
        print("Доступные COM порты:")
        for port in ports:
            print(f"  {port.device}: {port.description}")
        
        return None
    
    def connect(self):
        """Установка соединения с Arduino"""
        if not self.port:
            self.port = self.auto_detect_port()
            if not self.port:
                print("Arduino не найден. Укажите порт вручную.")
                return False
        
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            time.sleep(2)  # Даем Arduino время на инициализацию
            print(f"Соединение установлено: {self.port}")
            return True
        except Exception as e:
            print(f"Ошибка соединения: {e}")
            return False
    
    def send_command(self, command):
        """Отправка команды на Arduino"""
        if self.ser and self.ser.is_open:
            self.ser.write(f"{command}\n".encode('utf-8'))
            print(f"Отправлена команда: {command}")
    
    def read_json_message(self):
        """Чтение JSON сообщения от Arduino"""
        if self.ser and self.ser.in_waiting > 0:
            try:
                line = self.ser.readline().decode('utf-8').strip()
                if line:
                    return json.loads(line)
            except json.JSONDecodeError as e:
                print(f"Ошибка разбора JSON: {line}")
                return None
            except UnicodeDecodeError:
                return None
        return None
    
    def start_measurement(self):
        """Начать измерения"""
        self.send_command("START")
    
    def request_data(self):
        """Запросить данные измерений"""
        self.send_command("SEND")
    
    def get_status(self):
        """Получить статус"""
        self.send_command("STATUS")
    
    def reset(self):
        """Сбросить измерения"""
        self.send_command("RESET")
    
    def process_data(self, data):
        """Обработка полученных усредненных данных"""
        if "session_id" in data:
            self.session_id = data["session_id"]
        
        if "avg_response_us" in data and "rms_response_us" in data:
            # Сохраняем усредненные данные
            measurements = {
                'session_id': data.get('session_id', 'unknown'),
                'timestamp': datetime.now(),
                'avg_response_us': data['avg_response_us'],
                'rms_response_us': data['rms_response_us'],
                'avg_period_us': data['avg_period_us'],
                'rms_period_us': data['rms_period_us'],
                'statistics': data.get('statistics', {})
            }
            
            self.data.append(measurements)
            
            # Выводим статистику
            stats = data.get('statistics', {})
            if stats:
                print("\n" + "="*60)
                print(f"СЕССИЯ: {data.get('session_id', 'N/A')}")
                print(f"ГРУПП: {data.get('groups_count', 'N/A')} по {data.get('measurements_per_group', 'N/A')} измерений")
                print(f"ВРЕМЯ: {datetime.now().strftime('%H:%M:%S')}")
                print("-"*60)
                
                resp_stats = stats.get('response_time', {})
                if resp_stats:
                    print("ОБЩАЯ СТАТИСТИКА ВРЕМЕНИ ОТКЛИКА (мкс):")
                    print(f"  Среднее по группам: {resp_stats.get('overall_avg_us', 0):.2f}")
                    print(f"  Среднее СКО по группам: {resp_stats.get('overall_rms_us', 0):.2f}")
                
                per_stats = stats.get('period', {})
                if per_stats:
                    print("\nОБЩАЯ СТАТИСТИКА ПЕРИОДА (мкс):")
                    print(f"  Среднее по группам: {per_stats.get('overall_avg_us', 0):.2f}")
                    print(f"  Среднее СКО по группам: {per_stats.get('overall_rms_us', 0):.2f}")
                print("="*60 + "\n")
            
            # Сохраняем в файл
            self.save_to_file(data)
            
            # Предлагаем построить график
            self.plot_data(data)
            
            return True
        
        return False
    
    def plot_data(self, data):
        """Построение 4 графиков для усредненных данных"""
        if "avg_response_us" not in data:
            return
        
        avg_response = data['avg_response_us']
        rms_response = data['rms_response_us']
        avg_period = data['avg_period_us']
        rms_period = data['rms_period_us']
        
        # Создаем 4 графика (2x2)
        fig, axes = plt.subplots(ncols=2, nrows=2, figsize=(14, 10), layout="constrained")
        fig.suptitle(f"Усредненные измерения - Сессия {data.get('session_id', 'N/A')}")
        
        # 1. График среднего времени отклика по группам
        axes[0, 0].plot(range(len(avg_response)), avg_response, 'b.-', markersize=8, linewidth=2)
        axes[0, 0].set_title('Среднее время отклика по группам')
        axes[0, 0].set_xlabel('Номер группы')
        axes[0, 0].set_ylabel('Время (мкс)')
        axes[0, 0].grid(True, alpha=0.3)
        # Добавляем значения на точки
        for i, v in enumerate(avg_response):
            axes[0, 0].text(i, v, f'{v:.1f}', ha='center', va='bottom', fontsize=8)
        
        # 2. График СКО времени отклика по группам
        axes[0, 1].plot(range(len(rms_response)), rms_response, 'r.-', markersize=8, linewidth=2)
        axes[0, 1].set_title('СКО времени отклика по группам')
        axes[0, 1].set_xlabel('Номер группы')
        axes[0, 1].set_ylabel('СКО (мкс)')
        axes[0, 1].grid(True, alpha=0.3)
        # Добавляем значения на точки
        for i, v in enumerate(rms_response):
            axes[0, 1].text(i, v, f'{v:.2f}', ha='center', va='bottom', fontsize=8)
        
        # 3. График среднего периода по группам
        axes[1, 0].plot(range(len(avg_period)), avg_period, 'g.-', markersize=8, linewidth=2)
        axes[1, 0].set_title('Средний период по группам')
        axes[1, 0].set_xlabel('Номер группы')
        axes[1, 0].set_ylabel('Период (мкс)')
        axes[1, 0].grid(True, alpha=0.3)
        # Добавляем значения на точки
        for i, v in enumerate(avg_period):
            axes[1, 0].text(i, v, f'{v:.1f}', ha='center', va='bottom', fontsize=8)
        
        # 4. График СКО периода по группам
        axes[1, 1].plot(range(len(rms_period)), rms_period, 'm.-', markersize=8, linewidth=2)
        axes[1, 1].set_title('СКО периода по группам')
        axes[1, 1].set_xlabel('Номер группы')
        axes[1, 1].set_ylabel('СКО (мкс)')
        axes[1, 1].grid(True, alpha=0.3)
        # Добавляем значения на точки
        for i, v in enumerate(rms_period):
            axes[1, 1].text(i, v, f'{v:.2f}', ha='center', va='bottom', fontsize=8)
        
        plt.tight_layout()
        
        # Сохраняем график
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        plot_filename = f"arduino_measurements/4plots_session_{data.get('session_id', 'unknown')}_{timestamp}.png"
        plt.savefig(plot_filename, dpi=150)
        print(f"График (4 графика) сохранен: {plot_filename}")
        
        plt.show(block=False)

    def save_to_file(self, data):
        """Сохранение данных в файлы"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        session_id = data.get('session_id', 'unknown')
        
        # Создаем директорию для данных
        data_dir = "arduino_measurements"
        if not os.path.exists(data_dir):
            os.makedirs(data_dir)
        
        # Сохраняем полный JSON
        json_filename = f"{data_dir}/session_{session_id}_{timestamp}.json"
        with open(json_filename, 'w') as f:
            json.dump(data, f, indent=2)
        print(f"Данные сохранены в JSON: {json_filename}")
        
        # Сохраняем CSV с усредненными данными (если есть массивы avg и rms)
        if all(key in data for key in ['avg_response_us', 'rms_response_us', 'avg_period_us', 'rms_period_us']):
            # Создаем DataFrame с данными по группам
            groups_count = len(data['avg_response_us'])
            df_data = {
                'group_num': list(range(1, groups_count + 1)),
                'avg_response_us': data['avg_response_us'],
                'rms_response_us': data['rms_response_us'],
                'avg_period_us': data['avg_period_us'],
                'rms_period_us': data['rms_period_us']
            }
            
            # Добавляем общую статистику если есть
            if 'statistics' in data:
                stats = data['statistics']
                resp_stats = stats.get('response_time', {})
                per_stats = stats.get('period', {})
                
                df_data['overall_avg_response'] = [resp_stats.get('overall_avg_us', 0)] * groups_count
                df_data['overall_rms_response'] = [resp_stats.get('overall_rms_us', 0)] * groups_count
                df_data['overall_avg_period'] = [per_stats.get('overall_avg_us', 0)] * groups_count
                df_data['overall_rms_period'] = [per_stats.get('overall_rms_us', 0)] * groups_count
            
            df = pd.DataFrame(df_data)
            csv_filename = f"{data_dir}/session_{session_id}_{timestamp}.csv"
            df.to_csv(csv_filename, index=False)
            print(f"Усредненные данные сохранены в CSV: {csv_filename}")
        
    def interactive_mode(self):
        """Интерактивный режим работы"""
        print("\n" + "="*60)
        print("ИНТЕРАКТИВНЫЙ РЕЖИМ УПРАВЛЕНИЯ ARDUINO MEGA")
        print("="*60)
        print("Команды:")
        print("  start  - Начать измерения")
        print("  send   - Запросить данные")
        print("  status - Проверить статус")
        print("  reset  - Сбросить измерения")
        print("  plot   - Построить график последних данных")
        print("  save   - Сохранить все данные")
        print("  help   - Показать справку")
        print("  exit   - Выход")
        print("="*60)
        
        try:
            while True:
                # Читаем сообщения от Arduino
                message = self.read_json_message()
                if message:
                    print(f"\n[ARDUINO] {message.get('status', 'message')}: {message.get('message', '')}")
                    
                    # Если это данные измерений - обрабатываем
                    if "measurements_per_group" in message:
                        self.process_data(message)
                
                # Проверяем ввод пользователя
                if self.ser.in_waiting == 0:  # Только если нет данных от Arduino
                    cmd = input("\nВведите команду: ").strip().lower()
                    
                    if cmd == "start":
                        self.start_measurement()
                    elif cmd == "send":
                        self.request_data()
                    elif cmd == "status":
                        self.get_status()
                    elif cmd == "reset":
                        self.reset()
                    elif cmd == "plot" and self.data:
                        # Строим график по последним данным
                        self.plot_data(self.data[-1])
                    elif cmd == "save" and self.data:
                        # Сохраняем все данные
                        all_data_filename = f"arduino_measurements/all_sessions_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
                        with open(all_data_filename, 'w') as f:
                            json.dump(self.data, f, indent=2)
                        print(f"Все данные сохранены в: {all_data_filename}")
                    elif cmd == "help":
                        print("\nКоманды:")
                        print("  start  - Ардуино начинает отправку импульсов")
                        print("  send   - Ардуино отправляет собранные данные")
                        print("  status - Получить текущий статус измерений")
                        print("  reset  - Сбросить текущие измерения")
                        print("  plot   - Построить график последних данных")
                        print("  exit   - Завершить программу")
                    elif cmd == "exit":
                        print("Завершение работы...")
                        break
                    else:
                        print("Неизвестная команда. Введите 'help' для списка команд.")
                
                time.sleep(0.1)  # Небольшая задержка
                
        except KeyboardInterrupt:
            print("\nПрервано пользователем")
        finally:
            if self.ser:
                self.ser.close()
                print("Соединение закрыто")

def main():
    """Основная функция"""
    print("="*60)
    print("ПРИЕМНИК ДАННЫХ ДЛЯ ARDUINO MEGA <-> LICHEE RV DOCK")
    print("="*60)
    
    # Создаем экземпляр приемника
    receiver = ArduinoDataReceiver()
    
    # Подключаемся
    if not receiver.connect():
        # Если автоопределение не сработало, запрашиваем порт вручную
        port = input("Введите COM порт вручную (например COM3 или /dev/ttyACM0): ")
        receiver.port = port
        if not receiver.connect():
            return
    
    # Запускаем интерактивный режим
    receiver.interactive_mode()

if __name__ == "__main__":
    main()