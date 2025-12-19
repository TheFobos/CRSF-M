#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Тест установки каналов через API
"""

import sys
import time
import requests

def test_api_channels():
    """Тест установки каналов через API"""
    
    api_url = "http://localhost:8081"
    
    print("=" * 60)
    print("Тест установки каналов через API")
    print("=" * 60)
    print()
    
    print(f"Подключение к API серверу: {api_url}")
    
    # Проверка доступности API сервера
    try:
        response = requests.get(f"{api_url}/", timeout=2)
        print("[OK] API сервер доступен")
    except requests.exceptions.ConnectionError:
        print("[ERROR] Не удалось подключиться к API серверу")
        print("Убедитесь, что API сервер запущен:")
        print("  ./crsf_api_server 8081 <IP> 8082")
        return 1
    except Exception as e:
        print(f"[ERROR] Ошибка подключения: {e}")
        return 1
    
    print()
    
    # Установка режима manual
    print("1. Установка режима 'manual'...")
    try:
        print(f"   Отправка POST запроса на {api_url}/api/command/setMode...")
        response = requests.post(
            f"{api_url}/api/command/setMode",
            json={"mode": "manual"},
            timeout=10
        )
        print(f"   Статус ответа: {response.status_code}")
        print(f"   Содержимое ответа: {response.text[:200]}")
        response.raise_for_status()
        result = response.json()
        if result.get('status') == 'ok':
            print("   [OK] Режим 'manual' установлен")
        else:
            print(f"   [ERROR] Ошибка: {result.get('message', 'Unknown error')}")
            return 1
    except requests.exceptions.Timeout as e:
        print(f"   [ERROR] Таймаут запроса: {e}")
        print("   Возможно, API интерпретатор не запущен или не отвечает")
        return 1
    except Exception as e:
        print(f"   [ERROR] Ошибка установки режима: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    time.sleep(0.2)
    print()
    
    # Установка отдельных каналов
    print("2. Установка отдельных каналов...")
    test_channels = [
        (1, 1500),  # Roll - центр
        (2, 1000),  # Pitch - минимум
        (3, 2000),  # Throttle - максимум
        (4, 1500),  # Yaw - центр
    ]
    
    for channel, value in test_channels:
        try:
            response = requests.post(
                f"{api_url}/api/command/setChannel",
                json={"channel": channel, "value": value},
                timeout=10
            )
            response.raise_for_status()
            result = response.json()
            if result.get('status') == 'ok':
                print(f"   [OK] CH{channel} = {value}")
            else:
                print(f"   [ERROR] CH{channel}: {result.get('message', 'Unknown error')}")
        except Exception as e:
            print(f"   [ERROR] CH{channel}: {e}")
    
    time.sleep(0.2)
    print()
    
    # Установка всех каналов одновременно
    print("3. Установка всех 16 каналов одновременно...")
    all_channels = [1500] * 16
    all_channels[0] = 1200  # CH1
    all_channels[1] = 1800  # CH2
    all_channels[2] = 1000  # CH3 (Throttle минимум)
    all_channels[3] = 1500  # CH4
    
    try:
        response = requests.post(
            f"{api_url}/api/command/setChannels",
            json={"channels": all_channels},
            timeout=10
        )
        response.raise_for_status()
        result = response.json()
        if result.get('status') == 'ok':
            print("   [OK] Все каналы установлены")
            print(f"   CH1={all_channels[0]}, CH2={all_channels[1]}, CH3={all_channels[2]}, CH4={all_channels[3]}")
        else:
            print(f"   [ERROR] Ошибка: {result.get('message', 'Unknown error')}")
            return 1
    except Exception as e:
        print(f"   [ERROR] Ошибка установки каналов: {e}")
        return 1
    
    time.sleep(0.2)
    print()
    
    # Отправка каналов
    print("4. Отправка каналов...")
    try:
        response = requests.post(
            f"{api_url}/api/command/sendChannels",
            json={},
            timeout=10
        )
        response.raise_for_status()
        result = response.json()
        if result.get('status') == 'ok':
            print("   [OK] Каналы отправлены")
        else:
            print(f"   [ERROR] Ошибка: {result.get('message', 'Unknown error')}")
            return 1
    except Exception as e:
        print(f"   [ERROR] Ошибка отправки каналов: {e}")
        return 1
    
    print()
    print("=" * 60)
    print("Все тесты пройдены успешно!")
    print("=" * 60)
    
    return 0


if __name__ == "__main__":
    try:
        exit_code = test_api_channels()
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n\nТест прерван пользователем (Ctrl+C)")
        sys.exit(1)
