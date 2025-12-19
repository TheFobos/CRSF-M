#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Передача управления с джойстика на CRSF каналы через API

Эта программа:
1. Читает данные с джойстика (поддержка Linux /dev/input/jsX и Windows через pygame)
2. Преобразует значения осей джойстика в диапазон CRSF [1000..2000]
3. Отправляет значения на соответствующие каналы через HTTP API
4. Поддерживает чтение AUX каналов (aux1-aux4) с пультового джойстика

Требования:
- Установленный pygame: pip install pygame
- Запущенный CRSF API сервер

Использование:
    python3 joystick_to_api.py [опции]
"""

import sys
import time
import signal
import argparse
import threading
import queue
from typing import List, Set, Dict, Tuple
from collections import deque
from api_wrapper import CRSFAPIWrapper

# Попытка импортировать pygame
try:
    import pygame
    PYGAME_AVAILABLE = True
except ImportError:
    PYGAME_AVAILABLE = False
    print("⚠ pygame не найдена!")
    print("  Установите: pip install pygame")
    sys.exit(1)


# Значения по умолчанию
DEFAULT_API_URL = "http://localhost:8081"
DEFAULT_JOYSTICK_ID = 0
DEFAULT_UPDATE_RATE = 50  # Гц
DEFAULT_DEADZONE = 0.05  # 5% мертвая зона

# Маппинг осей джойстика на каналы CRSF
AXIS_TO_CHANNEL = {
    0: 4,  # Yaw
    1: 3,  # Throttle
    2: 1,  # Roll
    3: 2,  # Pitch
}

# Маппинг кнопок/осей на AUX каналы по умолчанию
DEFAULT_AUX_MAPPING = {
    'axis:4': (5, 'axis', False),  # AUX1
    'axis:5': (6, 'axis', False),  # AUX2
    'button:0': (7, 'button', False),  # AUX3
    'button:1': (8, 'button', False),  # AUX4
}

# Инверсия по умолчанию
DEFAULT_INVERTED_AXES = {1, 3}

# Конфигурация AUX каналов
AUX_CONFIG = []

# Глобальные переменные для корректного завершения
running = True
crsf = None

# Очередь для отправки каналов
send_queue = queue.Queue(maxsize=10)
send_lock = threading.Lock()


def signal_handler(sig, frame):
    """Обработчик сигнала для корректного завершения"""
    global running
    print("\n\nПолучен сигнал завершения, останавливаем программу...")
    running = False


def parse_aux_config(config_str: str) -> Tuple[str, int, int, bool, int, int, str]:
    """
    Парсит строку конфигурации AUX канала
    """
    parts = config_str.split(':')
    
    if len(parts) < 3:
        raise ValueError(f"Неверный формат конфигурации: {config_str}")
    
    src_type = parts[0].lower()
    src_num = int(parts[1])
    channel = int(parts[2])
    
    invert = False
    min_val = 1000
    max_val = 2000
    toggle_type = 'switch'
    
    if src_type == 'axis':
        if len(parts) >= 4:
            invert = parts[3].lower() == 'invert'
        min_val = 1000
        max_val = 2000
        toggle_type = 'range'
    
    elif src_type == 'button':
        if len(parts) >= 5:
            min_val = int(parts[3])
            max_val = int(parts[4])
            toggle_type = 'range'
        elif len(parts) >= 4:
            invert = parts[3].lower() == 'invert'
            min_val = 1000
            max_val = 2000
            toggle_type = 'switch'
    
    elif src_type == 'hat':
        if len(parts) >= 4:
            direction = parts[3].lower()
        min_val = 1000
        max_val = 2000
        toggle_type = 'hat'
    
    return (src_type, src_num, channel, invert, min_val, max_val, toggle_type)


def axis_to_crsf(value: float, deadzone: float = 0.0, invert: bool = False) -> int:
    """
    Преобразует значение оси джойстика [-1.0..1.0] в значение CRSF канала [1000..2000]
    """
    if invert:
        value = -value
    
    if abs(value) < deadzone:
        value = 0.0
    
    crsf_value = int(1500 + value * 500)
    
    if crsf_value < 1000:
        crsf_value = 1000
    elif crsf_value > 2000:
        crsf_value = 2000
    
    return crsf_value


def button_to_crsf(button_state: bool, min_val: int = 1000, max_val: int = 2000, invert: bool = False) -> int:
    """
    Преобразует состояние кнопки в значение CRSF канала
    """
    if invert:
        button_state = not button_state
    
    if button_state:
        return max_val
    else:
        return min_val


def hat_to_crsf(hat_value: Tuple[int, int], direction: str = 'x') -> int:
    """
    Преобразует значение хэта джойстика в значение CRSF канала
    """
    if direction == 'x':
        value = hat_value[0]
    else:
        value = hat_value[1]
    
    if value == -1:
        return 1000
    elif value == 1:
        return 2000
    else:
        return 1500


def init_joystick(joystick_id: int = 0):
    """
    Инициализирует джойстик
    """
    pygame.joystick.init()
    
    if pygame.joystick.get_count() == 0:
        print("✗ Джойстики не найдены!")
        return None
    
    if joystick_id >= pygame.joystick.get_count():
        print(f"✗ Джойстик с ID {joystick_id} не найден!")
        print(f"  Доступно джойстиков: {pygame.joystick.get_count()}")
        return None
    
    joystick = pygame.joystick.Joystick(joystick_id)
    joystick.init()
    
    print(f"✓ Джойстик подключен: {joystick.get_name()}")
    print(f"  Оси: {joystick.get_numaxes()}")
    print(f"  Кнопки: {joystick.get_numbuttons()}")
    print(f"  Шапки: {joystick.get_numhats()}")
    
    return joystick


def print_axis_mapping():
    """Выводит информацию о маппинге осей на каналы"""
    print("\nМаппинг осей джойстика на каналы CRSF:")
    channel_names = {
        1: "Roll",
        2: "Pitch",
        3: "Throttle",
        4: "Yaw",
        5: "AUX1",
        6: "AUX2",
        7: "AUX3",
        8: "AUX4",
    }
    
    for axis, channel in sorted(AXIS_TO_CHANNEL.items()):
        invert = " (инвертировано)" if axis in DEFAULT_INVERTED_AXES else ""
        ch_name = channel_names.get(channel, f"AUX{channel-4}")
        print(f"  Ось {axis} -> Канал {channel} ({ch_name}){invert}")
    
    if AUX_CONFIG:
        print("\nМаппинг AUX каналов:")
        for config in AUX_CONFIG:
            src_type, src_num, channel, invert, min_val, max_val, toggle_type = config
            ch_name = channel_names.get(channel, f"AUX{channel-4}")
            
            invert_str = " (инвертировано)" if invert else ""
            range_str = ""
            if toggle_type == 'range' and (min_val != 1000 or max_val != 2000):
                range_str = f" [{min_val}-{max_val}]"
            
            print(f"  {src_type.capitalize()} {src_num} -> Канал {channel} ({ch_name}){invert_str}{range_str}")


def send_worker():
    """Рабочий поток для отправки каналов"""
    global running, crsf
    
    while running:
        try:
            # Берем каналы из очереди с таймаутом
            channels = send_queue.get(timeout=0.01)
            
            # Отправляем каналы
            if channels and crsf:
                try:
                    # ВАЖНО: Всегда отправляем ВСЕ каналы, а не только измененные
                    crsf.set_channels(channels)
                    crsf.send_channels()
                    send_queue.task_done()
                    
                except Exception as e:
                    print(f"⚠ Ошибка отправки каналов: {e}")
                    send_queue.task_done()
                    
        except queue.Empty:
            # Очередь пуста, продолжаем
            continue
        except Exception as e:
            print(f"⚠ Ошибка в send_worker: {e}")
            time.sleep(0.01)


def main():
    """Основная функция"""
    global running, crsf, AUX_CONFIG
    
    parser = argparse.ArgumentParser(
        description='Передача управления с джойстика на CRSF каналы через API',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры использования:
  python3 joystick_to_api.py
  python3 joystick_to_api.py --api-url http://192.168.1.100:8081
  python3 joystick_to_api.py --joystick-id 0 --update-rate 100
  python3 joystick_to_api.py --api-url http://localhost:8081 --deadzone 0.05
  
Примеры настройки AUX каналов:
  python3 joystick_to_api.py --aux-config "axis:4:5"
  python3 joystick_to_api.py --aux-config "axis:4:5:invert"
  python3 joystick_to_api.py --aux-config "button:0:7"
  python3 joystick_to_api.py --aux-config "button:1:8:1000:1500"
  python3 joystick_to_api.py --aux-config "hat:0:9"
        """
    )
    
    parser.add_argument(
        '--api-url', '-a',
        type=str,
        default=DEFAULT_API_URL,
        help=f'URL API сервера (по умолчанию: {DEFAULT_API_URL})'
    )
    
    parser.add_argument(
        '--joystick-id', '-j',
        type=int,
        default=DEFAULT_JOYSTICK_ID,
        help=f'ID джойстика (по умолчанию: {DEFAULT_JOYSTICK_ID})'
    )
    
    parser.add_argument(
        '--update-rate', '-r',
        type=float,
        default=DEFAULT_UPDATE_RATE,
        help=f'Частота обновления в Гц (по умолчанию: {DEFAULT_UPDATE_RATE})'
    )
    
    parser.add_argument(
        '--deadzone', '-d',
        type=float,
        default=DEFAULT_DEADZONE,
        help=f'Мертвая зона для осей (0.0-1.0, по умолчанию: {DEFAULT_DEADZONE})'
    )
    
    parser.add_argument(
        '--invert-axis',
        type=int,
        action='append',
        default=[],
        help='Инвертировать ось (можно указать несколько раз, например --invert-axis 1 --invert-axis 3)'
    )
    
    parser.add_argument(
        '--aux-config',
        type=str,
        action='append',
        default=[],
        help='Конфигурация AUX каналов (формат: axis:channel[:invert], можно указать несколько раз)'
    )
    
    parser.add_argument(
        '--no-thread',
        action='store_true',
        help='Отключить многопоточность (отправка в основном потоке)'
    )
    
    args = parser.parse_args()
    
    # Проверяем параметры
    if args.deadzone < 0.0 or args.deadzone > 1.0:
        print("✗ Мертвая зона должна быть в диапазоне [0.0..1.0]")
        sys.exit(1)
    
    if args.update_rate <= 0:
        print("✗ Частота обновления должна быть больше 0")
        sys.exit(1)
    
    # Настраиваем инверсию осей
    inverted_axes: Set[int] = set(DEFAULT_INVERTED_AXES)
    if args.invert_axis:
        inverted_axes = set(args.invert_axis)
    
    # Парсим конфигурацию AUX каналов
    if args.aux_config:
        for config_str in args.aux_config:
            try:
                config = parse_aux_config(config_str)
                AUX_CONFIG.append(config)
                print(f"✓ Добавлен AUX канал: {config_str}")
            except Exception as e:
                print(f"✗ Ошибка парсинга конфигурации '{config_str}': {e}")
                sys.exit(1)
    else:
        # Используем конфигурацию по умолчанию
        for key, (channel, src_type, invert) in DEFAULT_AUX_MAPPING.items():
            src_type_str, src_num_str = key.split(':')
            src_num = int(src_num_str)
            config_str = f"{src_type_str}:{src_num}:{channel}"
            if invert:
                config_str += ":invert"
            try:
                config = parse_aux_config(config_str)
                AUX_CONFIG.append(config)
            except Exception as e:
                print(f"✗ Ошибка создания конфигурации по умолчанию: {e}")
    
    # Регистрируем обработчик сигналов
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    print("=" * 60)
    print("Передача управления с джойстика на CRSF каналы через API")
    print("=" * 60)
    print()
    
    # Инициализируем pygame
    print("Инициализация pygame...")
    pygame.init()
    # Устанавливаем минимальный набор событий
    pygame.event.set_allowed([
        pygame.JOYAXISMOTION,
        pygame.JOYBUTTONDOWN,
        pygame.JOYBUTTONUP,
        pygame.JOYHATMOTION,
        pygame.QUIT
    ])
    print("✓ pygame инициализирован")
    print()
    
    # Инициализируем джойстик
    print(f"Поиск джойстика (ID: {args.joystick_id})...")
    joystick = init_joystick(args.joystick_id)
    if joystick is None:
        sys.exit(1)
    print()
    
    # Проверяем доступность источников для AUX каналов
    for config in AUX_CONFIG:
        src_type, src_num, channel, invert, min_val, max_val, toggle_type = config
        
        if src_type == 'axis' and src_num >= joystick.get_numaxes():
            print(f"⚠ Внимание: Ось {src_num} не доступна на джойстике (доступно осей: {joystick.get_numaxes()})")
        elif src_type == 'button' and src_num >= joystick.get_numbuttons():
            print(f"⚠ Внимание: Кнопка {src_num} не доступна на джойстике (доступно кнопок: {joystick.get_numbuttons()})")
        elif src_type == 'hat' and src_num >= joystick.get_numhats():
            print(f"⚠ Внимание: Хэт {src_num} не доступна на джойстике (доступно хэтов: {joystick.get_numhats()})")
    
    # Выводим информацию о маппинге
    print_axis_mapping()
    if inverted_axes:
        print(f"\nИнвертированные оси: {sorted(inverted_axes)}")
    print()
    
    # Подключаемся к API
    print(f"Подключение к API серверу: {args.api_url}")
    try:
        crsf = CRSFAPIWrapper(args.api_url)
        crsf.auto_init()
        print("✓ Подключено к API серверу")
    except Exception as e:
        print(f"✗ Ошибка подключения к API серверу: {e}")
        print("\nУбедитесь, что:")
        print("  1. API сервер запущен: ./crsf_api_server 8081 <IP> 8082")
        print("  2. API интерпретатор запущен на ведомом узле")
        print("  3. URL API сервера правильный")
        sys.exit(1)
    print()
    
    # Устанавливаем режим manual для управления через API
    print("Установка режима 'manual'...")
    try:
        crsf.set_work_mode("manual")
        print("✓ Режим 'manual' установлен")
    except Exception as e:
        print(f"✗ Ошибка установки режима: {e}")
        sys.exit(1)
    print()
    
    # Запускаем поток для отправки
    send_thread = None
    if not args.no_thread:
        send_thread = threading.Thread(target=send_worker, daemon=True)
        send_thread.start()
        print("✓ Многопоточный режим включен")
    else:
        print("⚠ Многопоточный режим отключен (отправка в основном потоке)")
    print()
    
    # Вычисляем интервал обновления
    update_interval = 1.0 / args.update_rate
    
    print("=" * 60)
    print("Начало передачи управления")
    print(f"Частота обновления: {args.update_rate} Гц ({update_interval*1000:.1f} мс)")
    print(f"Мертвая зона: {args.deadzone*100:.1f}%")
    print(f"AUX каналов: {len(AUX_CONFIG)}")
    print("Нажмите Ctrl+C для остановки")
    print("=" * 60)
    print()
    
    # Инициализируем каналы нейтральными значениями
    channels = [1500] * 16
    
    # Основной цикл
    last_update_time = time.monotonic()
    iteration = 0
    last_axis_values = {}
    
    # Счетчик для отладки
    send_counter = 0
    last_print_time = time.monotonic()
    
    try:
        while running:
            # ВАЖНО: Всегда обрабатываем события
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
            
            # Сбрасываем флаг обновления
            channels_changed = False
            
            # Читаем ВСЕ оси и обновляем каналы
            for axis_id, channel_id in AXIS_TO_CHANNEL.items():
                if axis_id < joystick.get_numaxes():
                    axis_value = joystick.get_axis(axis_id)
                    
                    # Проверяем изменение
                    key = f"axis_{axis_id}"
                    if key not in last_axis_values or abs(last_axis_values[key] - axis_value) > 0.0001:
                        last_axis_values[key] = axis_value
                        
                        invert = axis_id in inverted_axes
                        crsf_value = axis_to_crsf(axis_value, args.deadzone, invert)
                        
                        if channels[channel_id - 1] != crsf_value:
                            channels[channel_id - 1] = crsf_value
                            channels_changed = True
            
            # Читаем AUX каналы
            for config in AUX_CONFIG:
                src_type, src_num, channel, invert, min_val, max_val, toggle_type = config
                
                try:
                    if src_type == 'axis' and src_num < joystick.get_numaxes():
                        axis_value = joystick.get_axis(src_num)
                        
                        # Проверяем изменение
                        key = f"aux_axis_{src_num}"
                        if key not in last_axis_values or abs(last_axis_values[key] - axis_value) > 0.0001:
                            last_axis_values[key] = axis_value
                            
                            new_value = axis_to_crsf(axis_value, args.deadzone, invert)
                            
                            # Масштабируем для пользовательского диапазона
                            if min_val != 1000 or max_val != 2000:
                                new_value = int(min_val + (max_val - min_val) * (new_value - 1000) / 1000)
                            
                            if channels[channel - 1] != new_value:
                                channels[channel - 1] = new_value
                                channels_changed = True
                    
                    elif src_type == 'button' and src_num < joystick.get_numbuttons():
                        button_state = joystick.get_button(src_num) == 1
                        new_value = button_to_crsf(button_state, min_val, max_val, invert)
                        
                        if channels[channel - 1] != new_value:
                            channels[channel - 1] = new_value
                            channels_changed = True
                    
                    elif src_type == 'hat' and src_num < joystick.get_numhats():
                        hat_value = joystick.get_hat(src_num)
                        new_value = hat_to_crsf(hat_value, 'x')
                        
                        if channels[channel - 1] != new_value:
                            channels[channel - 1] = new_value
                            channels_changed = True
                
                except Exception as e:
                    print(f"⚠ Ошибка чтения {src_type} {src_num}: {e}")
            
            # Проверяем время для отправки
            current_time = time.monotonic()
            time_since_last_update = current_time - last_update_time
            
            # ВАЖНО: Отправляем в двух случаях:
            # 1. Прошло достаточно времени (по частоте обновления)
            # 2. Или изменились каналы и мы хотим быстрый отклик
            if time_since_last_update >= update_interval or channels_changed:
                
                # Отправляем каналы
                if args.no_thread:
                    # Отправка в основном потоке
                    try:
                        crsf.set_channels(channels)
                        crsf.send_channels()
                        send_counter += 1
                    except Exception as e:
                        print(f"⚠ Ошибка отправки: {e}")
                else:
                    # Отправка через поток
                    try:
                        # Очищаем очередь от старых сообщений
                        while not send_queue.empty():
                            try:
                                send_queue.get_nowait()
                                send_queue.task_done()
                            except queue.Empty:
                                break
                        
                        # Добавляем новые каналы
                        send_queue.put_nowait(channels.copy())
                        send_counter += 1
                    except queue.Full:
                        # Очередь полна, пропускаем кадр
                        print("⚠ Очередь отправки переполнена, пропускаем кадр")
                    except Exception as e:
                        print(f"⚠ Ошибка добавления в очередь: {e}")
                
                # Обновляем время последней отправки
                last_update_time = current_time
                iteration += 1
            
            # Периодически выводим статус
            if current_time - last_print_time >= 1.0:
                elapsed = current_time - last_print_time
                actual_rate = send_counter / elapsed if elapsed > 0 else 0
                
                print(f"[{time.strftime('%H:%M:%S')}] "
                      f"CH1={channels[0]:4d} CH2={channels[1]:4d} "
                      f"CH3={channels[2]:4d} CH4={channels[3]:4d} "
                      f"Rate: {actual_rate:.1f} Hz", end='')
                
                # Выводим AUX каналы
                aux_channels = []
                for config in AUX_CONFIG:
                    src_type, src_num, channel, invert, min_val, max_val, toggle_type = config
                    if channel <= 8:
                        aux_channels.append(f"CH{channel}={channels[channel-1]:4d}")
                
                if aux_channels:
                    print(f" AUX: {' '.join(aux_channels)}")
                else:
                    print()
                
                # Сбрасываем счетчики
                send_counter = 0
                last_print_time = current_time
            
            # ВАЖНО: Не используем time.sleep() для лучшего отклика
            # Вместо этого используем небольшую задержку только если нужно
            if time_since_last_update < update_interval / 2:
                # Если до следующей отправки еще далеко, спим немного
                time_to_sleep = min(update_interval / 4, 0.001)
                time.sleep(time_to_sleep)
    
    except KeyboardInterrupt:
        print("\n\nПолучен сигнал прерывания (Ctrl+C)")
    
    finally:
        # Корректное завершение
        print("\nЗавершение работы...")
        
        # Ждем завершения очереди отправки
        if not args.no_thread:
            time.sleep(0.1)
            send_queue.join()
        
        # Устанавливаем все каналы в нейтральное положение
        print("Установка каналов в нейтральное положение...")
        try:
            neutral_channels = [1500] * 16
            crsf.set_channels(neutral_channels)
            crsf.send_channels()
            print("✓ Каналы установлены в нейтральное положение")
        except Exception as e:
            print(f"⚠ Ошибка при установке нейтральных значений: {e}")
        
        # Закрываем джойстик
        if joystick:
            joystick.quit()
        
        pygame.quit()
        print("✓ Программа завершена")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"\n\nКритическая ошибка: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)