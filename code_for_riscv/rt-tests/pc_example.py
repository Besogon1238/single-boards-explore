import serial
import json
import time
import pandas as pd
from datetime import datetime
import matplotlib as mpl
import matplotlib.pyplot as plt
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
        self.last_data_received = None
        
    def auto_detect_port(self):
        """Автоматическое определение порта Arduino"""
        import serial.tools.list_ports
        
        ports = list(serial.tools.list_ports.comports())
        for port in ports:
            if 'Arduino' in port.description or 'ACM' in port.description:
                print(f"Найден Arduino на порту: {port.device}")
                return port.device
        
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
            time.sleep(2)
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
            except json.JSONDecodeError:
                return None
            except UnicodeDecodeError:
                return None
        return None
    
    def start_measurement(self):
        """Начать измерения"""
        self.send_command("START")
    
    def request_data(self):
        """Запросить данные измерений и автоматически построить графики"""
        self.send_command("SEND")
        self.wait_and_process_data()
    
    def wait_and_process_data(self, timeout=10):
        """Ожидание данных и автоматическая обработка"""
        start_time = time.time()
        print("Ожидание данных от Arduino...")
        
        while time.time() - start_time < timeout:
            message = self.read_json_message()
            if message:
                if "avg_latency_us" in message:
                    self.process_data_with_plot(message)
                    return True
                else:
                    status = message.get('status', '')
                    if status == 'data_ready':
                        print(f"✓ [ARDUINO] {message.get('message', '')}")
                    elif status == 'sending':
                        print(f"✓ [ARDUINO] {message.get('message', '')}")
                    elif status == 'error':
                        print(f"✗ [ARDUINO] {message.get('message', '')}")
            
            time.sleep(0.1)
        
        print("Таймаут ожидания данных!")
        return False
    
    def get_status(self):
        """Получить статус"""
        self.send_command("STATUS")
    
    def reset(self):
        """Сбросить измерения"""
        self.send_command("RESET")
    
    def process_data_with_plot(self, data):
        """Обработка полученных данных и автоматическое построение графиков"""
        if "avg_latency_us" in data:
            self.process_mixed_data(data)
        else:
            print("Неизвестный формат данных")
            return False
    
    def process_mixed_data(self, data):
        if "session_id" in data:
            self.session_id = data["session_id"]
        
        # Сохраняем последние данные
        self.last_data_received = data
        
        # Сохраняем усредненные данные
        measurements = {
            'session_id': data.get('session_id', 'unknown'),
            'timestamp': datetime.now(),
            'avg_latency_us': data.get('avg_latency_us', []),
            'min_latency_us': data.get('min_latency_us', []),
            'max_latency_us': data.get('max_latency_us', []),
            'avg_jitter_us': data.get('avg_jitter_us', []),
            'statistics': data.get('statistics', {})
        }
        
        self.data.append(measurements)
        
        # Получаем параметры измерений
        params = data.get('parameters', {})
        delay_between_pulses = params.get('delay_between_pulses_us', 0)
        groups_count = data.get('groups_count', 0)
        measurements_per_group = data.get('measurements_per_group', 0)
        
        # Выводим статистику
        stats = data.get('statistics', {})
        if stats:
            print("\n" + "="*60)
            print(f"СЕССИЯ: {data.get('session_id', 'N/A')}")
            print(f"КОНФИГУРАЦИЯ: {groups_count} групп × {measurements_per_group} измерений")
            print(f"ЗАДЕРЖКА МЕЖДУ ИМПУЛЬСАМИ: {delay_between_pulses:,} µs")
            print(f"ВРЕМЯ: {datetime.now().strftime('%H:%M:%S')}")
            print("-"*60)
            
            latency_stats = stats.get('latency', {})
            if latency_stats:
                print("СТАТИСТИКА LATENCY (мкс):")
                print(f"  СРЕДНЯЯ (по всем группам): {latency_stats.get('overall_avg_us', 0):.2f}")
                print(f"  МИНИМАЛЬНАЯ: {latency_stats.get('overall_min_us', 0):.2f}")
                print(f"  МАКСИМАЛЬНАЯ: {latency_stats.get('overall_max_us', 0):.2f}")
                print(f"  РАЗБРОС: {latency_stats.get('variation_us', 0):.2f}")
            
            jitter_stats = stats.get('jitter', {})
            if jitter_stats:
                print(f"\nСТАТИСТИКА JITTER (мкс):")
                print(f"  СРЕДНИЙ (по всем группам): {jitter_stats.get('overall_avg_us', 0):.2f}")
            print("="*60 + "\n")
        
        # Автоматически строим графики
        self.generate_mixed_plots(data)
        
        # Сохраняем в файл
        self.save_mixed_data_to_file(data)
        
        return True
    
    def generate_mixed_plots(self, data):
        try:
            # Проверяем наличие необходимых данных
            if "avg_latency_us" not in data:
                print("Нет данных для построения графиков")
                return
            
            avg_latency = data['avg_latency_us']
            min_latency = data['min_latency_us']
            max_latency = data['max_latency_us']
            avg_jitter = data['avg_jitter_us']
            
            # Получаем общие средние значения из статистики
            stats = data.get('statistics', {})
            total_avg_latency = stats.get('latency', {}).get('overall_avg_us', 0)
            total_avg_jitter = stats.get('jitter', {}).get('overall_avg_us', 0)
            
            # Получаем параметры измерений
            params = data.get('parameters', {})
            delay_between_pulses = params.get('delay_between_pulses_us', 0)
            groups_count = data.get('groups_count', 0)
            measurements_per_group = data.get('measurements_per_group', 0)
            
            # Создаем директорию если ее нет
            data_dir = "arduino_measurements"
            if not os.path.exists(data_dir):
                os.makedirs(data_dir)
            
            # Создаем 2 графика (1x2)
            fig, axes = plt.subplots(1, 2, figsize=(14, 6))
            
            session_id = data.get('session_id', 'unknown')
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            
            # Обновленный заголовок с delay_between_pulses
            fig.suptitle(f'Latency & Jitter Analysis | Session: {session_id}\n'
                        f'Delay: {delay_between_pulses:,} µs | {timestamp}', 
                        fontsize=14, fontweight='bold')
            
            # Импортируем необходимый модуль
            from matplotlib.ticker import MaxNLocator
            
            # 1. График latency с диапазоном min-max
            if len(avg_latency) > 0:
                # Явно создаем список целых чисел для групп
                groups = list(range(1, len(avg_latency) + 1))
                
                # Основная линия - средние значения
                line1 = axes[0].plot(groups, avg_latency, 'b.-', markersize=8, linewidth=2, 
                                label=f'Средняя latency по группам')[0]
                
                # Область min-max (заливка)
                fill = axes[0].fill_between(groups, min_latency, max_latency, 
                                        alpha=0.2, color='blue', label='Диапазон min-max')
                
                # Точки min и max
                scatter_min = axes[0].scatter(groups, min_latency, color='green', s=20, 
                                            marker='^', alpha=0.6, label=f'Min: {min(min_latency):.1f} µs')
                scatter_max = axes[0].scatter(groups, max_latency, color='red', s=20, 
                                            marker='v', alpha=0.6, label=f'Max: {max(max_latency):.1f} µs')
                
                # Линия общего среднего значения latency (горизонтальная)
                axes[0].axhline(y=total_avg_latency, color='black', linestyle='--', 
                            linewidth=2, alpha=0.7, 
                            label=f'Общ. среднее: {total_avg_latency:.1f} µs')
                
                # Устанавливаем целочисленные метки на оси X
                axes[0].xaxis.set_major_locator(MaxNLocator(integer=True))
                axes[0].set_xticks(groups)  # Явно задаем позиции меток
                
                # Обновленный заголовок для первого графика с информацией о группах
                axes[0].set_title(f'Latency по группам ({groups_count}×{measurements_per_group} измерений)', 
                                fontsize=12)
                axes[0].set_xlabel('Номер группы')
                axes[0].set_ylabel('Latency (µs)')
                axes[0].grid(True, alpha=0.3)
                axes[0].legend(loc='best', fontsize=9)
            
            # 2. График среднего jitter (простая линия)
            if len(avg_jitter) > 0:
                # Используем тот же список групп
                groups = list(range(1, len(avg_jitter) + 1))
                
                # Простая линия для среднего jitter
                line2 = axes[1].plot(groups, avg_jitter, 'g.-', markersize=8, linewidth=2,
                                label='Средний jitter по группам')[0]
                
                # Линия общего среднего значения jitter (горизонтальная)
                axes[1].axhline(y=total_avg_jitter, color='black', linestyle='--', 
                            linewidth=2, alpha=0.7,
                            label=f'Общ. среднее: {total_avg_jitter:.1f} µs')
                
                # Устанавливаем целочисленные метки на оси X
                axes[1].xaxis.set_major_locator(MaxNLocator(integer=True))
                axes[1].set_xticks(groups)  # Явно задаем позиции меток
                
                # Обновленный заголовок для второго графика
                axes[1].set_title(f'Jitter по группам ({groups_count}×{measurements_per_group} измерений)', 
                                fontsize=12)
                axes[1].set_xlabel('Номер группы')
                axes[1].set_ylabel('Jitter (µs)')
                axes[1].grid(True, alpha=0.3)
                axes[1].legend(loc='best', fontsize=9)
            
            plt.tight_layout(rect=[0, 0, 1, 0.93])  # Немного увеличили отступ сверху для заголовка
            
            # Сохраняем график
            timestamp_file = datetime.now().strftime("%Y%m%d_%H%M%S")
            plot_filename = f"{data_dir}/latency_jitter_session_{session_id}_{timestamp_file}.png"
            plt.savefig(plot_filename, dpi=150)
            print(f"✓ Графики сохранены: {plot_filename}")
            
            # Показываем графики
            plt.show(block=False)
            plt.pause(0.1)
            
            # Закрываем фигуру
            time.sleep(3)
            plt.close(fig)
            
        except Exception as e:
            print(f"Ошибка при построении графиков: {e}")
            
    def save_mixed_data_to_file(self, data):
        """Сохранение данных в файлы"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            session_id = data.get('session_id', 'unknown')
            
            # Создаем директорию
            data_dir = "arduino_measurements"
            if not os.path.exists(data_dir):
                os.makedirs(data_dir)
            
            # Сохраняем JSON
            json_filename = f"{data_dir}/latency_jitter_session_{session_id}_{timestamp}.json"
            with open(json_filename, 'w') as f:
                json.dump(data, f, indent=2)
            print(f"✓ JSON сохранен: {json_filename}")
            
            # Сохраняем CSV если есть данные
            if all(key in data for key in ['avg_latency_us', 'min_latency_us', 'max_latency_us',
                                          'avg_jitter_us']):
                groups_count = len(data['avg_latency_us'])
                df_data = {
                    'group_num': list(range(1, groups_count + 1)),
                    'avg_latency_us': data['avg_latency_us'],
                    'min_latency_us': data['min_latency_us'],
                    'max_latency_us': data['max_latency_us'],
                    'latency_variation_us': [max_val - min_val for max_val, min_val in 
                                           zip(data['max_latency_us'], data['min_latency_us'])],
                    'avg_jitter_us': data['avg_jitter_us']
                }
                
                df = pd.DataFrame(df_data)
                csv_filename = f"{data_dir}/latency_jitter_session_{session_id}_{timestamp}.csv"
                df.to_csv(csv_filename, index=False)
                print(f"✓ CSV сохранен: {csv_filename}")
                
        except Exception as e:
            print(f"Ошибка при сохранении файлов: {e}")
    
    def interactive_mode(self):
        """Интерактивный режим работы"""
        print("\n" + "="*60)
        print("ИНТЕРАКТИВНЫЙ РЕЖИМ УПРАВЛЕНИЯ ARDUINO")
        print("="*60)
        print("Команды: start, send, status, reset, summary, save, help, exit")
        print("="*60)
        
        try:
            while True:
                # Читаем сообщения от Arduino
                message = self.read_json_message()
                if message:
                    status = message.get('status', '')
                    
                    if status == 'data_ready':
                        print(f"\n✓ [ARDUINO] {message.get('message', '')}")
                        print("   Используйте 'send' для получения данных и графиков")
                    elif status == 'started':
                        print(f"\n✓ [ARDUINO] {message.get('message', '')}")
                    elif status == 'sending':
                        print(f"\n✓ [ARDUINO] {message.get('message', '')}")
                    elif status == 'error':
                        print(f"\n✗ [ARDUINO] Ошибка: {message.get('message', '')}")
                    elif "avg_latency_us" in message:
                        print("\n✓ [ARDUINO] Получены данные измерений!")
                        # Автоматическая обработка
                        self.process_data_with_plot(message)
                
                # Проверяем ввод пользователя
                cmd = input("\nВведите команду: ").strip().lower()
                
                if cmd == "start":
                    self.start_measurement()
                elif cmd == "send":
                    print("Запрос данных...")
                    self.request_data()
                elif cmd == "status":
                    self.get_status()
                elif cmd == "reset":
                    self.reset()
                elif cmd == "summary":
                    self.create_summary_plot()
                elif cmd == "save" and self.data:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    all_data_filename = f"arduino_measurements/all_sessions_{timestamp}.json"
                    with open(all_data_filename, 'w') as f:
                        json.dump(self.data, f, indent=2)
                    print(f"✓ Все данные сохранены: {all_data_filename}")
                elif cmd == "help":
                    print("\n" + "="*40)
                    print("КОМАНДЫ:")
                    print("="*40)
                    print("start   - Начать измерения")
                    print("send    - Получить данные и графики (АВТОМАТИЧЕСКИ)")
                    print("status  - Статус измерений")
                    print("reset   - Сбросить измерения")
                    print("summary - График сравнения сессий")
                    print("save    - Сохранить все данные")
                    print("exit    - Выход")
                    print("="*40)
                elif cmd == "exit":
                    print("\nЗавершение работы...")
                    break
                else:
                    print("Неизвестная команда. Введите 'help'")
                
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\nПрервано пользователем")
        except Exception as e:
            print(f"\nОшибка: {e}")
        finally:
            if self.ser:
                self.ser.close()
                print("Соединение закрыто")
    
    def create_summary_plot(self):
        if not self.data:
            print("Нет данных для построения сводного графика")
            return
        
        try:
            fig, axes = plt.subplots(1, 2, figsize=(14, 6))
            
            # Импортируем необходимый модуль
            from matplotlib.ticker import MaxNLocator
            
            for i, session in enumerate(self.data):
                session_id = session.get('session_id', f'Session_{i}')
                
                # Проверяем тип данных
                if 'avg_latency_us' in session:
                    # Latency/Jitter данные
                    avg_latency = session['avg_latency_us']
                    avg_jitter = session['avg_jitter_us']
                    
                    if avg_latency and len(avg_latency) > 0:
                        groups = list(range(1, len(avg_latency) + 1))
                        axes[0].plot(groups, avg_latency, '.-', 
                                    label=session_id, alpha=0.7)
                    
                    if avg_jitter and len(avg_jitter) > 0:
                        groups = list(range(1, len(avg_jitter) + 1))
                        axes[1].plot(groups, avg_jitter, '.-', 
                                    label=session_id, alpha=0.7)
            
            # Устанавливаем целочисленные метки на обеих осях X
            axes[0].xaxis.set_major_locator(MaxNLocator(integer=True))
            axes[1].xaxis.set_major_locator(MaxNLocator(integer=True))
            
            axes[0].set_title('Сравнение среднего Latency между сессиями')
            axes[0].set_xlabel('Номер группы')
            axes[0].set_ylabel('Latency (µs)')
            axes[0].legend()
            axes[0].grid(True, alpha=0.3)
            
            axes[1].set_title('Сравнение среднего Jitter между сессиями')
            axes[1].set_xlabel('Номер группы')
            axes[1].set_ylabel('Jitter (µs)')
            axes[1].legend()
            axes[1].grid(True, alpha=0.3)
            
            plt.tight_layout()
            
            # Сохраняем
            data_dir = "arduino_measurements"
            if not os.path.exists(data_dir):
                os.makedirs(data_dir)
            
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            plt.savefig(f"{data_dir}/summary_comparison_{timestamp}.png", dpi=150)
            plt.show(block=False)
            print("✓ Сводный график сохранен")
            
            time.sleep(3)
            plt.close()
            
        except Exception as e:
            print(f"Ошибка при построении сводного графика: {e}")

def main():
    """Основная функция"""
    print("="*60)
    print("СИСТЕМА ИЗМЕРЕНИЯ ARDUINO <-> LICHEE")
    print("="*60)
    print("Автоматическое построение графиков при команде 'send'")
    print("="*60)
    
    receiver = ArduinoDataReceiver()
    
    if not receiver.connect():
        port = input("Введите COM порт вручную: ")
        receiver.port = port
        if not receiver.connect():
            return
    
    receiver.interactive_mode()

if __name__ == "__main__":
    main()