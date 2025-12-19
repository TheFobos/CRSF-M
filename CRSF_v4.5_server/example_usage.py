#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Простой консольный пример использования CRSFWrapper

Этот скрипт демонстрирует все основные возможности работы с CRSF через Python обертку.
Перед запуском убедитесь, что основное приложение crsf_io_rpi запущено.
"""

import sys
import os
import time

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


def print_telemetry(telemetry):
    """Красиво выводит телеметрию на экран"""
    print(f"  Связь установлена: {'ДА' if telemetry['linkUp'] else 'НЕТ'}")
    print(f"  Активный порт: {telemetry['activePort']}")
    print(f"  Время последнего приема: {telemetry['lastReceive']} мс")
    print(f"  Временная метка: {telemetry['timestamp']}")
    
    # Каналы
    print(f"\n  Каналы (16 штук):")
    channels = telemetry['channels']
    for i in range(0, 16, 4):  # Выводим по 4 канала в строке
        ch_str = "  ".join([f"CH{i+j+1:2d}={channels[i+j]:4d}" 
                           for j in range(min(4, 16-i))])
        print(f"    {ch_str}")
    
    # GPS данные
    gps = telemetry['gps']
    if gps['latitude'] != 0.0 or gps['longitude'] != 0.0:
        print(f"\n  GPS данные:")
        print(f"    Широта: {gps['latitude']:.6f}°")
        print(f"    Долгота: {gps['longitude']:.6f}°")
        print(f"    Высота: {gps['altitude']:.2f} м")
        print(f"    Скорость: {gps['speed']:.2f} м/с")
    
    # Данные батареи
    battery = telemetry['battery']
    if battery['voltage'] > 0:
        print(f"\n  Батарея:")
        print(f"    Напряжение: {battery['voltage']:.2f} В")
        print(f"    Ток: {battery['current']:.2f} А")
        print(f"    Емкость: {battery['capacity']:.2f} мАч")
        print(f"    Остаток: {battery['remaining']}%")
    
    # Углы наклона (attitude)
    attitude = telemetry['attitude']
    if attitude['roll'] != 0.0 or attitude['pitch'] != 0.0:
        print(f"\n  Углы наклона:")
        print(f"    Roll (крен): {attitude['roll']:.2f}°")
        print(f"    Pitch (тангаж): {attitude['pitch']:.2f}°")
        print(f"    Yaw (рыскание): {attitude['yaw']:.2f}°")


def main():
    """Основная функция - демонстрация всех возможностей CRSFWrapper"""
    
    print_separator("Пример использования CRSFWrapper")
    print("\nУбедитесь, что основное приложение crsf_io_rpi запущено!")
    print("Ожидание 2 секунды...\n")
    time.sleep(2)
    
    # ============================================================
    # ШАГ 1: СОЗДАНИЕ И ИНИЦИАЛИЗАЦИЯ CRSFWrapper
    # ============================================================
    print_separator("ШАГ 1: Инициализация CRSFWrapper")
    
    try:
        # Создаем экземпляр обертки
        crsf = CRSFWrapper()
        print("  ✓ CRSFWrapper создан")
        
        # Автоматическая инициализация (проверяет наличие файла с телеметрией)
        crsf.auto_init()
        print("  ✓ Автоматическая инициализация выполнена")
        print("  ✓ CRSF готов к работе!")
        
    except RuntimeError as e:
        print(f"  ✗ ОШИБКА: {e}")
        print("\n  Решение: Убедитесь, что crsf_io_rpi запущен!")
        sys.exit(1)
    
    # ============================================================
    # ШАГ 2: ПОЛУЧЕНИЕ ТЕЛЕМЕТРИИ
    # ============================================================
    print_separator("ШАГ 2: Получение телеметрии")
    
    try:
        # Получаем телеметрию
        telemetry = crsf.get_telemetry()
        print("  ✓ Телеметрия получена успешно\n")
        
        # Выводим телеметрию на экран
        print_telemetry(telemetry)
        
        # Проверяем состояние связи
        if not telemetry['linkUp']:
            print("\n  ⚠ ПРЕДУПРЕЖДЕНИЕ: Связь с CRSF приемником не установлена!")
            print("     Это нормально, если приемник не подключен или не настроен.")
            print("     Пакеты будут показываться как 'потерянные' пока связь не установится.")
            print("     Каналы можно устанавливать в любом случае.")
        
    except Exception as e:
        print(f"  ✗ ОШИБКА при получении телеметрии: {e}")
    
    # ============================================================
    # ШАГ 3: РАБОТА С РЕЖИМАМИ
    # ============================================================
    print_separator("ШАГ 3: Работа с режимами")
    
    try:
        # Получаем текущий режим работы
        current_mode = crsf.get_work_mode()
        print(f"  Текущий режим работы: {current_mode}")
        
        # Устанавливаем режим "joystick" (управление через джойстик/приемник)
        print("\n  Устанавливаем режим 'joystick'...")
        crsf.set_work_mode("joystick")
        print("  ✓ Режим 'joystick' установлен")
        
        # Проверяем что режим изменился
        new_mode = crsf.get_work_mode()
        print(f"  Проверка: текущий режим = {new_mode}")
        
        # Устанавливаем режим "manual" (ручное управление)
        print("\n  Устанавливаем режим 'manual'...")
        crsf.set_work_mode("manual")
        print("  ✓ Режим 'manual' установлен")
        
        # Проверяем что режим изменился
        final_mode = crsf.get_work_mode()
        print(f"  Проверка: текущий режим = {final_mode}")
        
    except Exception as e:
        print(f"  ✗ ОШИБКА при работе с режимами: {e}")
    
    # ============================================================
    # ШАГ 4: РАБОТА С ОТДЕЛЬНЫМИ КАНАЛАМИ
    # ============================================================
    print_separator("ШАГ 4: Управление отдельными каналами")
    
    try:
        # ВАЖНО: Для установки каналов через Python обертку нужно 
        # переключиться в режим "manual", иначе каналы будут 
        # перезаписываться джойстиком в режиме "joystick"
        print("  Устанавливаем режим 'manual' для управления каналами...")
        crsf.set_work_mode("manual")
        time.sleep(0.1)  # Даем время основному приложению обработать команду
        
        # Устанавливаем значение одного канала
        # Каналы нумеруются от 1 до 16
        # Значения от 1000 до 2000 (1500 = нейтральное положение)
        
        print("\n  Устанавливаем значения для нескольких каналов:")
        
        # Канал 1: минимальное значение (1000)
        crsf.set_channel(1, 1000)
        print("    ✓ CH1 = 1000 (минимум)")
        
        # Канал 2: максимальное значение (2000)
        crsf.set_channel(2, 2000)
        print("    ✓ CH2 = 2000 (максимум)")
        
        # Канал 3: нейтральное значение (1500)
        crsf.set_channel(3, 1500)
        print("    ✓ CH3 = 1500 (нейтраль)")
        
        # Канал 4: произвольное значение
        crsf.set_channel(4, 1750)
        print("    ✓ CH4 = 1750")
        
        # Отправляем каналы (команды записываются в файл)
        # Основное приложение читает файл /tmp/crsf_command.txt и обрабатывает команды
        print("\n  Отправляем каналы...")
        crsf.send_channels()
        print("  ✓ Команды отправлены (записаны в файл)")
        
        # Подождем, чтобы основное приложение успело обработать команды
        # Основное приложение обрабатывает команды в главном цикле (~10 мс)
        print("\n  Ждем 0.5 секунды для обработки команд основным приложением...")
        time.sleep(0.5)
        
        # Проверяем телеметрию
        telemetry = crsf.get_telemetry()
        print("\n  Проверяем результат:")
        print(f"    CH1 в телеметрии: {telemetry['channels'][0]} (ожидалось: 1000)")
        print(f"    CH2 в телеметрии: {telemetry['channels'][1]} (ожидалось: 2000)")
        print(f"    CH3 в телеметрии: {telemetry['channels'][2]} (ожидалось: 1500)")
        print(f"    CH4 в телеметрии: {telemetry['channels'][3]} (ожидалось: 1750)")
        
        # Проверяем, что каналы действительно установились
        if (telemetry['channels'][0] == 1000 and 
            telemetry['channels'][1] == 2000 and 
            telemetry['channels'][2] == 1500 and 
            telemetry['channels'][3] == 1750):
            print("  ✓ Все каналы установлены правильно!")
        else:
            print("  ⚠ Каналы не совпадают с ожидаемыми значениями")
            print("     Возможно, основное приложение еще не обработало команды,")
            print("     или режим не переключен в 'manual'")
        
    except Exception as e:
        print(f"  ✗ ОШИБКА при работе с каналами: {e}")
    
    # ============================================================
    # ШАГ 5: УСТАНОВКА ВСЕХ КАНАЛОВ СРАЗУ
    # ============================================================
    print_separator("ШАГ 5: Установка всех 16 каналов одновременно")
    
    try:
        # Убеждаемся, что режим все еще "manual"
        current_mode = crsf.get_work_mode()
        if current_mode != "manual":
            print("  Переключаемся в режим 'manual'...")
            crsf.set_work_mode("manual")
            time.sleep(0.1)
        
        # Создаем список из 16 значений каналов
        # Значения должны быть от 1000 до 2000
        all_channels = [
            1500,  # CH1: нейтраль
            1500,  # CH2: нейтраль
            1500,  # CH3: нейтраль
            1500,  # CH4: нейтраль
            1500,  # CH5: нейтраль
            1500,  # CH6: нейтраль
            1500,  # CH7: нейтраль
            1500,  # CH8: нейтраль
            1500,  # CH9: нейтраль
            1500,  # CH10: нейтраль
            1500,  # CH11: нейтраль
            1500,  # CH12: нейтраль
            1500,  # CH13: нейтраль
            1500,  # CH14: нейтраль
            1500,  # CH15: нейтраль
            1500   # CH16: нейтраль
        ]
        
        # Устанавливаем все каналы одновременно
        print("  Устанавливаем все 16 каналов в нейтральное положение (1500)...")
        crsf.set_channels(all_channels)
        print("  ✓ Все каналы установлены")
        
        # Отправляем каналы
        crsf.send_channels()
        print("  ✓ Команды отправлены (записаны в файл)")
        
        # Ждем обработки команд
        time.sleep(0.5)
        
        # Пример: устанавливаем каналы для квадрокоптера
        # (имитация управления газом, рулями и т.д.)
        print("\n  Устанавливаем каналы для примера управления квадрокоптером:")
        quad_channels = [
            1500,  # CH1: Roll (крен) - нейтраль
            1500,  # CH2: Pitch (тангаж) - нейтраль
            1000,  # CH3: Throttle (газ) - минимум
            1500,  # CH4: Yaw (рыскание) - нейтраль
            1500,  # CH5: Aux1
            1500,  # CH6: Aux2
            1500,  # CH7: Aux3
            1500,  # CH8: Aux4
            1500,  # CH9: Aux5
            1500,  # CH10: Aux6
            1500,  # CH11: Aux7
            1500,  # CH12: Aux8
            1500,  # CH13: Aux9
            1500,  # CH14: Aux10
            1500,  # CH15: Aux11
            1500   # CH16: Aux12
        ]
        crsf.set_channels(quad_channels)
        print("  ✓ Каналы для квадрокоптера установлены")
        print("    (CH3 = 1000 - газ на минимуме для безопасности)")
        
        # Отправляем каналы
        crsf.send_channels()
        print("  ✓ Команды отправлены")
        
        # Ждем обработки
        time.sleep(0.5)
        
        # Проверяем результат
        telemetry = crsf.get_telemetry()
        print("\n  Проверяем CH3 (Throttle) в телеметрии:")
        print(f"    CH3 = {telemetry['channels'][2]} (ожидалось: 1000)")
        if telemetry['channels'][2] == 1000:
            print("  ✓ Канал CH3 установлен правильно!")
        else:
            print("  ⚠ Канал CH3 не совпадает с ожидаемым значением")
        
    except Exception as e:
        print(f"  ✗ ОШИБКА при установке всех каналов: {e}")
    
    # ============================================================
    # ЗАКЛЮЧЕНИЕ
    # ============================================================
    print_separator("Завершение работы")
    print("\n  ✓ Все примеры выполнены успешно!")
    print("\n  Краткая справка по использованию:")
    print("    - crsf.auto_init() - инициализация")
    print("    - crsf.get_telemetry() - получить телеметрию")
    print("    - crsf.set_work_mode('joystick'|'manual') - установить режим")
    print("    - crsf.get_work_mode() - получить режим")
    print("    - crsf.set_channel(1-16, 1000-2000) - установить канал")
    print("    - crsf.set_channels([список 16 значений]) - установить все каналы")
    print("    - crsf.send_channels() - отправить каналы")
    print("\n" + "=" * 60)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nПрограмма прервана пользователем (Ctrl+C)")
        sys.exit(0)
    except Exception as e:
        print(f"\n\nКритическая ошибка: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

