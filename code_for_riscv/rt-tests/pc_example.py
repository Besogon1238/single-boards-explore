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
                # Проверяем, есть ли ключи latency в данных
                if "avg_latency_us" in message or "avg_response_us" in message:
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
        # Определяем тип данных (старая или новая версия)
        if "avg_latency_us" in data:
            # Новая версия с latency/jitter
            self.process_latency_jitter_data(data)
        elif "avg_response_us" in data:
            # Старая версия с response/period
            self.process_response_period_data(data)
        else:
            print("Неизвестный формат данных")
            return False
    
    def process_latency_jitter_data(self, data):
        """Обработка данных latency/jitter"""
        if "session_id" in data:
            self.session_id = data["session_id"]
        
        # Сохраняем последние данные
        self.last_data_received = data
        
        # Сохраняем усредненные данные
        measurements = {
            'session_id': data.get('session_id', 'unknown'),
            'timestamp': datetime.now(),
            'avg_latency_us': data.get('avg_latency_us', []),
            'rms_latency_us': data.get('rms_latency_us', []),
            'avg_jitter_us': data.get('avg_jitter_us', []),
            'rms_jitter_us': data.get('rms_jitter_us', []),
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
            
            latency_stats = stats.get('latency', {})
            if latency_stats:
                print("ОБЩАЯ СТАТИСТИКА LATENCY (мкс):")
                print(f"  Средняя задержка: {latency_stats.get('overall_avg_us', 0):.2f}")
                print(f"  Среднее СКО: {latency_stats.get('overall_rms_us', 0):.2f}")
            
            jitter_stats = stats.get('jitter', {})
            if jitter_stats:
                print("\nОБЩАЯ СТАТИСТИКА JITTER (мкс):")
                print(f"  Средний джиттер: {jitter_stats.get('overall_avg_us', 0):.2f}")
                print(f"  Среднее СКО: {jitter_stats.get('overall_rms_us', 0):.2f}")
            print("="*60 + "\n")
        
        # Автоматически строим графики
        self.generate_latency_jitter_plots(data)
        
        # Сохраняем в файл
        self.save_latency_jitter_to_file(data)
        
        return True
    
    def process_response_period_data(self, data):
        """Обработка старых данных response/period (обратная совместимость)"""
        if "session_id" in data:
            self.session_id = data["session_id"]
        
        self.last_data_received = data
        
        measurements = {
            'session_id': data.get('session_id', 'unknown'),
            'timestamp': datetime.now(),
            'avg_response_us': data.get('avg_response_us', []),
            'rms_response_us': data.get('rms_response_us', []),
            'avg_period_us': data.get('avg_period_us', []),
            'rms_period_us': data.get('rms_period_us', []),
            'statistics': data.get('statistics', {})
        }
        
        self.data.append(measurements)
        
        # Выводим статистику
        stats = data.get('statistics', {})
        if stats:
            print("\n" + "="*60)
            print(f"СЕССИЯ: {data.get('session_id', 'N/A')}")
            print(f"ГРУПП: {data.get('groups_count', 'N/A')} по {data.get('measurements_per_group', 'N/A')} измерений")
            print("-"*60)
            
            resp_stats = stats.get('response_time', {})
            if resp_stats:
                print("ОБЩАЯ СТАТИСТИКА RESPONSE TIME (мкс):")
                print(f"  Среднее: {resp_stats.get('overall_avg_us', 0):.2f}")
                print(f"  СКО: {resp_stats.get('overall_rms_us', 0):.2f}")
            
            per_stats = stats.get('period', {})
            if per_stats:
                print("\nОБЩАЯ СТАТИСТИКА PERIOD (мкс):")
                print(f"  Среднее: {per_stats.get('overall_avg_us', 0):.2f}")
                print(f"  СКО: {per_stats.get('overall_rms_us', 0):.2f}")
            print("="*60 + "\n")
        
        # Строим графики для старого формата
        self.generate_response_period_plots(data)
        self.save_response_period_to_file(data)
        
        return True
    
    def generate_latency_jitter_plots(self, data):
        """Построение графиков для latency/jitter"""
        try:
            # Проверяем наличие необходимых данных
            if "avg_latency_us" not in data:
                print("Нет данных для построения графиков latency/jitter")
                return
            
            avg_latency = data['avg_latency_us']
            rms_latency = data['rms_latency_us']
            avg_jitter = data['avg_jitter_us']
            rms_jitter = data['rms_jitter_us']
            
            # Создаем директорию если ее нет
            data_dir = "arduino_measurements"
            if not os.path.exists(data_dir):
                os.makedirs(data_dir)
            
            # Создаем 4 графика (2x2)
            fig, axes = plt.subplots(ncols=2, nrows=2, figsize=(14, 10))
            
            session_id = data.get('session_id', 'unknown')
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            fig.suptitle(f'Latency & Jitter Analysis\nSession: {session_id} | {timestamp}', 
                        fontsize=14, fontweight='bold')
            
            # 1. График средней latency
            if len(avg_latency) > 0:
                axes[0, 0].plot(range(1, len(avg_latency) + 1), avg_latency, 'b.-', markersize=8, linewidth=2)
                axes[0, 0].set_title('Average Latency per Group', fontsize=12)
                axes[0, 0].set_xlabel('Group Number')
                axes[0, 0].set_ylabel('Latency (µs)')
                axes[0, 0].grid(True, alpha=0.3)
                for i, v in enumerate(avg_latency):
                    axes[0, 0].text(i + 1, v, f'{v:.1f}', ha='center', va='bottom', fontsize=8)
            
            # 2. График СКО latency
            if len(rms_latency) > 0:
                axes[0, 1].plot(range(1, len(rms_latency) + 1), rms_latency, 'r.-', markersize=8, linewidth=2)
                axes[0, 1].set_title('Latency RMS per Group', fontsize=12)
                axes[0, 1].set_xlabel('Group Number')
                axes[0, 1].set_ylabel('RMS (µs)')
                axes[0, 1].grid(True, alpha=0.3)
                for i, v in enumerate(rms_latency):
                    axes[0, 1].text(i + 1, v, f'{v:.2f}', ha='center', va='bottom', fontsize=8)
            
            # 3. График среднего jitter
            if len(avg_jitter) > 0:
                axes[1, 0].plot(range(1, len(avg_jitter) + 1), avg_jitter, 'g.-', markersize=8, linewidth=2)
                axes[1, 0].set_title('Average Jitter per Group', fontsize=12)
                axes[1, 0].set_xlabel('Group Number')
                axes[1, 0].set_ylabel('Jitter (µs)')
                axes[1, 0].grid(True, alpha=0.3)
                for i, v in enumerate(avg_jitter):
                    axes[1, 0].text(i + 1, v, f'{v:.1f}', ha='center', va='bottom', fontsize=8)
            
            # 4. График СКО jitter
            if len(rms_jitter) > 0:
                axes[1, 1].plot(range(1, len(rms_jitter) + 1), rms_jitter, 'm.-', markersize=8, linewidth=2)
                axes[1, 1].set_title('Jitter RMS per Group', fontsize=12)
                axes[1, 1].set_xlabel('Group Number')
                axes[1, 1].set_ylabel('RMS (µs)')
                axes[1, 1].grid(True, alpha=0.3)
                for i, v in enumerate(rms_jitter):
                    axes[1, 1].text(i + 1, v, f'{v:.2f}', ha='center', va='bottom', fontsize=8)
            
            plt.tight_layout()
            
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
    
    def generate_response_period_plots(self, data):
        """Построение графиков для старого формата response/period"""
        try:
            if "avg_response_us" not in data:
                print("Нет данных для построения графиков response/period")
                return
            
            avg_response = data['avg_response_us']
            rms_response = data['rms_response_us']
            avg_period = data['avg_period_us']
            rms_period = data['rms_period_us']
            
            # Создаем директорию если ее нет
            data_dir = "arduino_measurements"
            if not os.path.exists(data_dir):
                os.makedirs(data_dir)
            
            # Создаем графики
            fig, axes = plt.subplots(ncols=2, nrows=2, figsize=(14, 10))
            
            session_id = data.get('session_id', 'unknown')
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            fig.suptitle(f'Response Time & Period Analysis\nSession: {session_id} | {timestamp}', 
                        fontsize=14, fontweight='bold')
            
            # 1. График среднего response time
            if len(avg_response) > 0:
                axes[0, 0].plot(range(1, len(avg_response) + 1), avg_response, 'b.-', markersize=8, linewidth=2)
                axes[0, 0].set_title('Average Response Time per Group', fontsize=12)
                axes[0, 0].set_xlabel('Group Number')
                axes[0, 0].set_ylabel('Response Time (µs)')
                axes[0, 0].grid(True, alpha=0.3)
            
            # 2. График СКО response time
            if len(rms_response) > 0:
                axes[0, 1].plot(range(1, len(rms_response) + 1), rms_response, 'r.-', markersize=8, linewidth=2)
                axes[0, 1].set_title('Response Time RMS per Group', fontsize=12)
                axes[0, 1].set_xlabel('Group Number')
                axes[0, 1].set_ylabel('RMS (µs)')
                axes[0, 1].grid(True, alpha=0.3)
            
            # 3. График среднего периода
            if len(avg_period) > 0:
                axes[1, 0].plot(range(1, len(avg_period) + 1), avg_period, 'g.-', markersize=8, linewidth=2)
                axes[1, 0].set_title('Average Period per Group', fontsize=12)
                axes[1, 0].set_xlabel('Group Number')
                axes[1, 0].set_ylabel('Period (µs)')
                axes[1, 0].grid(True, alpha=0.3)
            
            # 4. График СКО периода
            if len(rms_period) > 0:
                axes[1, 1].plot(range(1, len(rms_period) + 1), rms_period, 'm.-', markersize=8, linewidth=2)
                axes[1, 1].set_title('Period RMS per Group', fontsize=12)
                axes[1, 1].set_xlabel('Group Number')
                axes[1, 1].set_ylabel('RMS (µs)')
                axes[1, 1].grid(True, alpha=0.3)
            
            plt.tight_layout()
            
            # Сохраняем график
            timestamp_file = datetime.now().strftime("%Y%m%d_%H%M%S")
            plot_filename = f"{data_dir}/response_period_session_{session_id}_{timestamp_file}.png"
            plt.savefig(plot_filename, dpi=150)
            print(f"✓ Графики сохранены: {plot_filename}")
            
            plt.show(block=False)
            plt.pause(0.1)
            time.sleep(3)
            plt.close(fig)
            
        except Exception as e:
            print(f"Ошибка при построении графиков: {e}")
    
    def save_latency_jitter_to_file(self, data):
        """Сохранение данных latency/jitter в файлы"""
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
            if all(key in data for key in ['avg_latency_us', 'rms_latency_us', 
                                          'avg_jitter_us', 'rms_jitter_us']):
                groups_count = len(data['avg_latency_us'])
                df_data = {
                    'group_num': list(range(1, groups_count + 1)),
                    'avg_latency_us': data['avg_latency_us'],
                    'rms_latency_us': data['rms_latency_us'],
                    'avg_jitter_us': data['avg_jitter_us'],
                    'rms_jitter_us': data['rms_jitter_us']
                }
                
                df = pd.DataFrame(df_data)
                csv_filename = f"{data_dir}/latency_jitter_session_{session_id}_{timestamp}.csv"
                df.to_csv(csv_filename, index=False)
                print(f"✓ CSV сохранен: {csv_filename}")
                
        except Exception as e:
            print(f"Ошибка при сохранении файлов: {e}")
    
    def save_response_period_to_file(self, data):
        """Сохранение старых данных response/period"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            session_id = data.get('session_id', 'unknown')
            
            data_dir = "arduino_measurements"
            if not os.path.exists(data_dir):
                os.makedirs(data_dir)
            
            # JSON
            json_filename = f"{data_dir}/response_period_session_{session_id}_{timestamp}.json"
            with open(json_filename, 'w') as f:
                json.dump(data, f, indent=2)
            print(f"✓ JSON сохранен: {json_filename}")
            
            # CSV
            if all(key in data for key in ['avg_response_us', 'rms_response_us',
                                          'avg_period_us', 'rms_period_us']):
                groups_count = len(data['avg_response_us'])
                df_data = {
                    'group_num': list(range(1, groups_count + 1)),
                    'avg_response_us': data['avg_response_us'],
                    'rms_response_us': data['rms_response_us'],
                    'avg_period_us': data['avg_period_us'],
                    'rms_period_us': data['rms_period_us']
                }
                
                df = pd.DataFrame(df_data)
                csv_filename = f"{data_dir}/response_period_session_{session_id}_{timestamp}.csv"
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
                    elif "avg_latency_us" in message or "avg_response_us" in message:
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
        """Сводный график всех сессий"""
        if not self.data:
            print("Нет данных для построения сводного графика")
            return
        
        try:
            fig, axes = plt.subplots(2, 2, figsize=(14, 10))
            
            for i, session in enumerate(self.data):
                session_id = session.get('session_id', f'Session_{i}')
                
                # Проверяем тип данных
                if 'avg_latency_us' in session:
                    # Latency/Jitter данные
                    avg_latency = session['avg_latency_us']
                    avg_jitter = session['avg_jitter_us']
                    
                    if avg_latency and len(avg_latency) > 0:
                        axes[0, 0].plot(range(len(avg_latency)), avg_latency, '.-', 
                                      label=session_id, alpha=0.7)
                    
                    if avg_jitter and len(avg_jitter) > 0:
                        axes[0, 1].plot(range(len(avg_jitter)), avg_jitter, '.-', 
                                      label=session_id, alpha=0.7)
                
                elif 'avg_response_us' in session:
                    # Response/Period данные
                    avg_response = session['avg_response_us']
                    avg_period = session['avg_period_us']
                    
                    if avg_response and len(avg_response) > 0:
                        axes[0, 0].plot(range(len(avg_response)), avg_response, '.-', 
                                      label=session_id, alpha=0.7)
                    
                    if avg_period and len(avg_period) > 0:
                        axes[0, 1].plot(range(len(avg_period)), avg_period, '.-', 
                                      label=session_id, alpha=0.7)
            
            axes[0, 0].set_title('Сравнение Latency/Response между сессиями')
            axes[0, 0].set_xlabel('Номер группы')
            axes[0, 0].set_ylabel('µs')
            axes[0, 0].legend()
            axes[0, 0].grid(True, alpha=0.3)
            
            axes[0, 1].set_title('Сравнение Jitter/Period между сессиями')
            axes[0, 1].set_xlabel('Номер группы')
            axes[0, 1].set_ylabel('µs')
            axes[0, 1].legend()
            axes[0, 1].grid(True, alpha=0.3)
            
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