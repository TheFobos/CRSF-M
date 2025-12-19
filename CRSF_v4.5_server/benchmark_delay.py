#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Бенчмарк задержки отправки и получения каналов CRSF

Этот скрипт измеряет задержку от установки канала через Python обертку
до момента получения обновленного значения в телеметрии.
Значения каналов выбираются случайно в диапазоне 1000-2000.
Перед запуском убедитесь, что основное приложение crsf_io_rpi запущено.
"""

import sys
import os
import time
import argparse
import random

# Добавляем путь к pybind для импорта обертки
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pybind'))
from crsf_wrapper import CRSFWrapper


def print_separator(text=""):
    """Печатает разделитель для красивого вывода"""
    if text:
        print(f"\n{'=' * 60}")
        print(f"  {text}")
        print('=' * 60)
    else:
        print('-' * 60)


def main(num_tests=5):
    """Основная функция бенчмарка
    
    Args:
        num_tests: Количество тестов для выполнения (по умолчанию 5)
    """
    
    print_separator("Бенчмарк задержки CRSF")
    print(f"\nКоличество тестов: {num_tests}")
    print("  Значения каналов будут выбраны случайно в диапазоне 1000-2000")
    
    # Предупреждение о времени выполнения при большом количестве тестов
    # Каждый тест занимает минимум ~1.2 секунды (1 сек таймаут + 0.2 сек пауза)
    estimated_time = num_tests * 1.2
    if estimated_time > 60:
        minutes = int(estimated_time / 60)
        seconds = int(estimated_time % 60)
        print(f"  ⚠ Предупреждение: Примерное время выполнения: ~{minutes} мин {seconds} сек")
        if estimated_time > 300:  # Больше 5 минут
            print(f"  ⚠ Это может занять очень много времени! Рекомендуется использовать меньше тестов.")
    
    print("Убедитесь, что основное приложение crsf_io_rpi запущено!")
    print("Ожидание 2 секунды...\n")
    time.sleep(2)
    
    # Инициализация CRSFWrapper
    print_separator("Инициализация CRSFWrapper")
    
    try:
        crsf = CRSFWrapper()
        print("  ✓ CRSFWrapper создан")
        
        crsf.auto_init()
        print("  ✓ Автоматическая инициализация выполнена")
        print("  ✓ CRSF готов к работе!")
        
    except RuntimeError as e:
        print(f"  ✗ ОШИБКА: {e}")
        print("\n  Решение: Убедитесь, что crsf_io_rpi запущен!")
        sys.exit(1)
    
    # Бенчмарк задержки
    print_separator("Бенчмарк задержки отправки и получения каналов")
    
    try:
        print("  Измеряем задержку от установки канала до получения в телеметрии")
        print("  (время от crsf.set_channel() до обновления в crsf.get_telemetry())\n")
        
        # Убеждаемся, что режим "manual"
        current_mode = crsf.get_work_mode()
        if current_mode != "manual":
            print("  Переключаемся в режим 'manual'...")
            crsf.set_work_mode("manual")
            time.sleep(0.1)
        
        # Генерируем случайные тестовые значения для канала (диапазон 1000-2000)
        min_value = 1000
        max_value = 2000
        
        # Инициализируем генератор случайных чисел для воспроизводимости (опционально)
        random.seed()
        
        # Генерируем случайные значения
        test_values = [random.randint(min_value, max_value) for _ in range(num_tests)]
        
        print(f"  Сгенерировано {num_tests} случайных значений в диапазоне {min_value}-{max_value}")
        
        delays = []
        
        for test_num, test_value in enumerate(test_values, 1):
            print(f"\n  Тест {test_num}/{num_tests}: Установка CH1 = {test_value}")
            
            # Получаем начальное значение для сравнения
            initial_telemetry = crsf.get_telemetry()
            initial_value = initial_telemetry['channels'][0] if initial_telemetry.get('channels') and len(initial_telemetry['channels']) > 0 else None
            
            # Засекаем время отправки
            send_time = time.time()
            
            # Устанавливаем канал
            crsf.set_channel(1, test_value)
            crsf.send_channels()
            
            print(f"    Время отправки: {send_time:.6f} сек")
            
            # Ждем появления значения в телеметрии
            max_wait_time = 1.0  # Максимальное время ожидания 1 секунда
            check_interval = 0.001  # Проверяем каждую миллисекунду
            receive_time = None
            iterations = 0
            max_iterations = int(max_wait_time / check_interval)
            current_value = None
            
            while iterations < max_iterations:
                telemetry = crsf.get_telemetry()
                
                # Проверяем, что список каналов не пустой
                if not telemetry.get('channels') or len(telemetry['channels']) == 0:
                    time.sleep(check_interval)
                    iterations += 1
                    continue
                
                current_value = telemetry['channels'][0]
                
                if current_value == test_value:
                    receive_time = time.time()
                    break
                
                time.sleep(check_interval)
                iterations += 1
            
            if receive_time:
                delay = (receive_time - send_time) * 1000  # Конвертируем в миллисекунды
                delays.append(delay)
                print(f"    Время получения: {receive_time:.6f} сек")
                print(f"    ✓ Задержка: {delay:.2f} мс")
                print(f"    Значение в телеметрии: CH1 = {current_value} (ожидалось: {test_value})")
            else:
                print(f"    ✗ Таймаут: значение не появилось в телеметрии за {max_wait_time} сек")
                # Безопасное получение текущего значения
                final_telemetry = crsf.get_telemetry()
                if final_telemetry.get('channels') and len(final_telemetry['channels']) > 0:
                    print(f"    Текущее значение CH1: {final_telemetry['channels'][0]}")
                else:
                    print(f"    Текущее значение CH1: недоступно (каналы пусты)")
            
            # Небольшая пауза между тестами
            time.sleep(0.2)
        
        # Выводим статистику
        if delays:
            print("\n" + "=" * 60)
            print("  РЕЗУЛЬТАТЫ БЕНЧМАРКА:")
            print("=" * 60)
            print(f"  Количество успешных тестов: {len(delays)}/{len(test_values)}")
            print(f"  Минимальная задержка: {min(delays):.2f} мс")
            print(f"  Максимальная задержка: {max(delays):.2f} мс")
            avg_delay = sum(delays) / len(delays)
            print(f"  Средняя задержка: {avg_delay:.2f} мс")
            
            # Медиана
            sorted_delays = sorted(delays)
            mid = len(sorted_delays) // 2
            if len(sorted_delays) % 2 == 0:
                median = (sorted_delays[mid - 1] + sorted_delays[mid]) / 2
            else:
                median = sorted_delays[mid]
            print(f"  Медианная задержка: {median:.2f} мс")
            
            print("\n  Примечание:")
            print("    Задержка включает:")
            print("    - Запись команды в файл /tmp/crsf_command.txt")
            print("    - Чтение файла основным приложением")
            print("    - Установку канала в CrsfSerial")
            print("    - Отправку пакета каналов (если включено)")
            print("    - Запись телеметрии в файл /tmp/crsf_telemetry.dat")
            print("    - Чтение файла Python оберткой")
        else:
            print("\n  ✗ Бенчмарк не выполнен: ни один тест не завершился успешно")
        
    except Exception as e:
        print(f"\n  ✗ ОШИБКА при выполнении бенчмарка: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    
    print_separator("Завершение бенчмарка")
    print("\n  ✓ Бенчмарк завершен успешно!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Бенчмарк задержки отправки и получения каналов CRSF",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры использования:
  %(prog)s              # Запуск с 5 тестами (по умолчанию)
  %(prog)s -n 10        # Запуск с 10 тестами
  %(prog)s --num-tests 1000 # Запуск с 1000 тестами
  
Примечание:
  - Значения каналов выбираются случайно в диапазоне 1000-2000
  - Количество тестов не ограничено (значения могут повторяться)
  - При большом количестве тестов (>250) будет запрошено подтверждение
        """
    )
    parser.add_argument(
        '-n', '--num-tests',
        type=int,
        default=5,
        metavar='N',
        help='Количество тестов для выполнения (по умолчанию: 5)'
    )
    
    args = parser.parse_args()
    
    # Проверяем, что количество тестов положительное
    if args.num_tests < 1:
        print("Ошибка: количество тестов должно быть больше 0")
        sys.exit(1)
    
    # Предупреждение о времени выполнения при большом количестве тестов
    estimated_time = args.num_tests * 1.2  # секунды
    if estimated_time > 300:  # Больше 5 минут
        minutes = int(estimated_time / 60)
        print(f"⚠ Предупреждение: Примерное время выполнения: ~{minutes} минут")
        print(f"  Это может занять очень много времени!")
        print(f"  Рекомендуется использовать меньше тестов (например, 100-500)")
        response = input("  Продолжить? (y/N): ").strip().lower()
        if response not in ['y', 'yes', 'да']:
            print("Отменено пользователем.")
            sys.exit(0)
        print()
    
    try:
        main(args.num_tests)
    except KeyboardInterrupt:
        print("\n\nПрограмма прервана пользователем (Ctrl+C)")
        sys.exit(0)
    except Exception as e:
        print(f"\n\nКритическая ошибка: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

