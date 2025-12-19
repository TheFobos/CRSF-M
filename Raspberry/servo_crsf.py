import time
import os
from gpiozero import Servo
from gpiozero.pins.pigpio import PiGPIOFactory

class CRSFReader:
    def __init__(self, servo_pin=18, min_pulse=0.0005, max_pulse=0.0025):
        """
        Инициализация CRSF ридера и сервопривода
        servo_pin: GPIO пин для сервопривода (по умолчанию 17)
        min_pulse: минимальная длительность импульса
        max_pulse: максимальная длительность импульса
        """
        # Используем pigpio для более плавного управления
        factory = PiGPIOFactory()
        self.servo = Servo(servo_pin,
                          min_pulse_width=min_pulse,
                          max_pulse_width=max_pulse,
                          pin_factory=factory)
        
        self.file_path = "/tmp/crsf_command.txt"
        self.last_position = None
        
        # Три основных позиции сервопривода
        self.positions = {
            1000: "min",     # 0 градусов
            1500: "mid",     # 90 градусов
            2000: "max"      # 180 градусов
        }
        
    def parse_channel_values(self, line):
        """
        Парсинг строки вида: setChannels 1=1858 2=1500 3=1500 ... 16=1500
        Возвращает словарь с значениями каналов
        """
        channels = {}
        
        try:
            # Удаляем возможные лишние символы
            line = line.strip()
            
            # Ищем команду setChannels
            if not line.startswith("setChannels"):
                return channels
            
            # Разбиваем строку на части
            parts = line.split()
            
            # Первая часть - "setChannels", остальные - каналы
            for part in parts[1:]:
                if '=' in part:
                    channel_str, value_str = part.split('=')
                    try:
                        channel = int(channel_str)
                        value = int(value_str)
                        channels[channel] = value
                    except ValueError:
                        continue
                        
        except Exception as e:
            print(f"Ошибка парсинга строки: {e}")
            
        return channels
    
    def get_channel_5_value(self):
        """
        Чтение файла и получение последнего значения 5-го канала
        """
        try:
            if not os.path.exists(self.file_path):
                return None
                
            with open(self.file_path, 'r') as file:
                lines = file.readlines()
            
            # Ищем последнюю команду setChannels
            for line in reversed(lines):
                line = line.strip()
                if line.startswith("setChannels"):
                    channels = self.parse_channel_values(line)
                    
                    # Возвращаем значение 5-го канала, если оно есть
                    if 5 in channels:
                        return channels[5]
                    else:
                        return None
                        
            return None
            
        except Exception as e:
            print(f"Ошибка при чтении файла: {e}")
            return None
    
    def map_to_position(self, crsf_value):
        """
        Преобразование значения CRSF в ближайшую стандартную позицию
        """
        # Ограничиваем значение
        crsf_value = max(1000, min(2000, crsf_value))
        
        # Определяем ближайшую стандартную позицию
        closest_pos = min(self.positions.keys(), 
                         key=lambda x: abs(x - crsf_value))
        
        return closest_pos, self.positions[closest_pos]
    
    def set_servo_position(self, position_name):
        """
        Установка сервопривода в указанную позицию
        """
        if position_name == "min":
            self.servo.min()
        elif position_name == "mid":
            self.servo.mid()
        elif position_name == "max":
            self.servo.max()
        else:
            print(f"Неизвестная позиция: {position_name}")
    
    def process_crsf_value(self, crsf_value):
        """
        Обработка значения CRSF и управление сервоприводом
        """
        if crsf_value is None:
            return
            
        # Определяем позицию
        position_value, position_name = self.map_to_position(crsf_value)
        
        # Если позиция изменилась
        if self.last_position != position_value:
            # Рассчитываем примерный угол
            angle = (crsf_value - 1000) / 1000 * 180
            
            print(f"Канал 5: {crsf_value} -> {position_name} (~{angle:.0f}°)")
            
            # Устанавливаем сервопривод в нужную позицию
            self.set_servo_position(position_name)
            self.last_position = position_value
    
    def run(self, update_interval=0.1):
        """
        Основной цикл программы
        """
        print("=" * 60)
        print("CRSF Reader и Servo Controller")
        print("=" * 60)
        print(f"Чтение файла: {self.file_path}")
        print(f"Управление сервоприводом на пине GPIO {self.servo.pin}")
        print("Ожидание команд формата:")
        print("  setChannels 1=1858 2=1500 3=1500 ... 5=XXX ... 16=1500")
        print("\nДиапазон значений для канала 5:")
        print("  1000-1333 -> servo.min()  (0 градусов)")
        print("  1334-1666 -> servo.mid()  (90 градусов)")
        print("  1667-2000 -> servo.max()  (180 градусов)")
        print("=" * 60)
        print("Нажмите Ctrl+C для выхода\n")
        
        try:
            while True:
                # Получаем значение 5-го канала
                crsf_value = self.get_channel_5_value()
                
                # Обрабатываем значение
                self.process_crsf_value(crsf_value)
                
                # Небольшая задержка для снижения нагрузки на CPU
                time.sleep(update_interval)
                
        except KeyboardInterrupt:
            print("\n" + "=" * 60)
            print("Выход из программы")
            print("Отключаем сервопривод...")
            self.servo.value = None  # Отключаем сервопривод
            print("Программа завершена")
            print("=" * 60)

# Упрощенная версия для тестирования
class SimpleCRSFReader:
    def __init__(self, servo_pin=18):
        """
        Упрощенная версия без pigpio
        """
        self.servo = Servo(servo_pin)
        self.file_path = "/tmp/crsf_command.txt"
        self.last_value = None
        
    def parse_line(self, line):
        """
        Парсинг строки с каналами
        """
        if not line.startswith("setChannels"):
            return None
            
        # Ищем значение 5-го канала
        parts = line.split()
        for part in parts:
            if part.startswith('5='):
                try:
                    value = int(part.split('=')[1])
                    return value
                except ValueError:
                    return None
        return None
    
    def run(self):
        print("Простая версия CRSF Reader")
        print("Ожидание команд в формате setChannels...")
        
        try:
            while True:
                # Читаем файл
                try:
                    with open(self.file_path, 'r') as f:
                        lines = f.readlines()
                        
                    # Ищем последнюю команду setChannels
                    for line in reversed(lines):
                        line = line.strip()
                        if line.startswith("setChannels"):
                            value = self.parse_line(line)
                            
                            if value is not None:
                                # Проверяем изменилось ли значение
                                if self.last_value != value:
                                    self.last_value = value
                                    
                                    # Устанавливаем позицию сервопривода
                                    if 1000 <= value <= 1333:
                                        print(f"Канал 5: {value} -> servo.min()")
                                        self.servo.min()
                                    elif 1334 <= value <= 1666:
                                        print(f"Канал 5: {value} -> servo.mid()")
                                        self.servo.mid()
                                    elif 1667 <= value <= 2000:
                                        print(f"Канал 5: {value} -> servo.max()")
                                        self.servo.max()
                                    
                                break
                except FileNotFoundError:
                    if self.last_value is None:
                        print(f"Ожидание файла: {self.file_path}")
                    time.sleep(1)
                except Exception as e:
                    print(f"Ошибка: {e}")
                    
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\nОтключаем сервопривод...")
            self.servo.value = None

# Функция для мониторинга всех каналов
def monitor_crsf_channels():
    """
    Функция для отладки - показывает все каналы
    """
    file_path = "/tmp/crsf_command.txt"
    
    print("Мониторинг CRSF каналов")
    print("=" * 60)
    
    try:
        while True:
            try:
                with open(file_path, 'r') as f:
                    lines = f.readlines()
                    
                # Ищем последнюю команду setChannels
                for line in reversed(lines):
                    line = line.strip()
                    if line.startswith("setChannels"):
                        print("\n" + time.strftime("%H:%M:%S"))
                        print("-" * 40)
                        
                        # Парсим все каналы
                        parts = line.split()
                        for part in parts[1:]:  # Пропускаем "setChannels"
                            if '=' in part:
                                chan, val = part.split('=')
                                channel_num = int(chan)
                                value = int(val)
                                
                                # Выделяем 5-й канал цветом
                                if channel_num == 5:
                                    print(f"\033[92mКанал {chan}: {value}\033[0m ← УПРАВЛЕНИЕ")
                                else:
                                    print(f"Канал {chan}: {value}")
                        
                        break
                        
            except FileNotFoundError:
                print("Файл не найден, ожидание...")
                time.sleep(2)
            except Exception as e:
                print(f"Ошибка: {e}")
                
            time.sleep(0.5)
            
    except KeyboardInterrupt:
        print("\nМониторинг остановлен")

# Тестовая функция для проверки формата команд
def test_parser():
    """
    Тестирование парсера команд
    """
    test_lines = [
        "setChannels 1=1858 2=1500 3=1500 4=1500 5=1500 6=1500 7=1500 8=1500 9=1500 10=1500 11=1500 12=1500 13=1500 14=1500 15=1500 16=1500",
        "setChannels 1=1000 2=2000 3=1500 4=1500 5=1200 6=1500 7=1500 8=1500",
        "sendChannels",
        "setChannels 5=1800 6=1200 7=1500",
    ]
    
    reader = CRSFReader()
    
    print("Тест парсера команд:")
    print("=" * 60)
    
    for line in test_lines:
        print(f"\nСтрока: {line}")
        if line.startswith("setChannels"):
            channels = reader.parse_channel_values(line)
            print(f"Распарсенные каналы: {channels}")
            if 5 in channels:
                print(f"Канал 5: {channels[5]}")
        else:
            print("Пропуск (не setChannels)")

# Основная программа
if __name__ == "__main__":
    import sys
    
    print("CRSF Reader and Servo Controller")
    print("Версия 3.0 (поддержка формата setChannels)")
    print()
    
    # Парсинг аргументов командной строки
    args = sys.argv[1:]
    pin = 18  # Пин по умолчанию
    
    # Проверяем аргументы
    if "--pin" in args:
        try:
            idx = args.index("--pin")
            pin = int(args[idx + 1])
        except (ValueError, IndexError):
            print("Ошибка: неверный номер пина")
            pin = 18
    
    # Выбор режима работы
    if "--monitor" in args or "--debug" in args:
        # Режим мониторинга всех каналов
        monitor_crsf_channels()
    elif "--test-parser" in args:
        # Тестирование парсера
        test_parser()
    elif "--simple" in args:
        # Упрощенный режим
        reader = SimpleCRSFReader(pin)
        reader.run()
    else:
        try:
            # Основной режим с pigpio
            print("Запуск в основном режиме (с pigpio)...")
            reader = CRSFReader(servo_pin=pin)
            reader.run()
        except Exception as e:
            print(f"Не удалось запустить основной режим: {e}")
            print("Запускаю упрощенную версию...")
            reader = SimpleCRSFReader(pin)
            reader.run()