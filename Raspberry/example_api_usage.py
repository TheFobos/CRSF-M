#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Пример использования CRSF через API
Демонстрирует отправку команд через HTTP API
"""

import sys
import time
from api_wrapper import CRSFAPIWrapper


def main():
    """Основная функция - демонстрация работы через API"""
    
    print("=" * 60)
    print("Пример использования CRSF через API")
    print("=" * 60)
    print()
    
    # URL API сервера (по умолчанию localhost:8081)
    api_url = "http://localhost:8081"
    if len(sys.argv) > 1:
        api_url = sys.argv[1]
    
    print(f"Подключение к API серверу: {api_url}")
    print()
    
    try:
        # Создаем обертку для API
        crsf = CRSFAPIWrapper(api_url)
        
        # Инициализация (проверяет доступность API сервера)
        print("Инициализация...")
        crsf.auto_init()
        print("✓ Подключено к API серверу")
        print()
        
        # Установка режима manual
        print("Установка режима 'manual'...")
        crsf.set_work_mode("manual")
        print("✓ Режим 'manual' установлен")
        print()
        
        # Установка каналов
        print("Установка каналов...")
        crsf.set_channel(1, 1500)  # Канал 1 в нейтральное положение
        crsf.set_channel(2, 1500)  # Канал 2 в нейтральное положение
        crsf.set_channel(3, 1000)  # Канал 3 (газ) на минимум
        crsf.set_channel(4, 1500)  # Канал 4 в нейтральное положение
        print("✓ Каналы установлены")
        print()
        
        # Отправка каналов
        print("Отправка каналов...")
        crsf.send_channels()
        print("✓ Каналы отправлены")
        print()
        
        # Пример: установка всех каналов одновременно
        print("Установка всех 16 каналов...")
        all_channels = [1500] * 16  # Все каналы в нейтральное положение
        all_channels[2] = 1000  # Канал 3 (газ) на минимум
        crsf.set_channels(all_channels)
        crsf.send_channels()
        print("✓ Все каналы установлены и отправлены")
        print()
        
        print("=" * 60)
        print("Пример выполнен успешно!")
        print("=" * 60)
        print()
        print("Дополнительные команды:")
        print("  crsf.set_work_mode('joystick')  # Переключить в режим джойстика")
        print("  crsf.set_channel(номер, значение)  # Установить канал (1-16, 1000-2000)")
        print("  crsf.set_channels([список 16 значений])  # Установить все каналы")
        print("  crsf.send_channels()  # Отправить каналы")
        print()
        
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        print()
        print("Убедитесь, что:")
        print("  1. API сервер запущен: ./crsf_api_server 8081 <IP> 8082")
        print("  2. API интерпретатор запущен на ведомом узле: ./crsf_api_interpreter 8082")
        print("  3. Основное приложение запущено на ведомом узле: sudo ./crsf_io_rpi")
        print("  4. URL API сервера правильный")
        sys.exit(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nПрограмма прервана пользователем (Ctrl+C)")
        sys.exit(0)

