#!/usr/bin/env python3
"""
Тест стабильности API сервера
"""

import requests
import time
import statistics

def test_stability(api_url, duration=10, frequency=20):
    """Тестирует стабильность API сервера"""
    print(f"Тест стабильности API: {api_url}")
    print(f"Длительность: {duration} сек, Частота: {frequency} Гц")
    print("-" * 60)
    
    channels = [1500] * 16
    interval = 1.0 / frequency
    
    latencies = []
    successes = 0
    failures = 0
    
    start_time = time.time()
    end_time = start_time + duration
    
    test_count = 0
    
    while time.time() < end_time:
        test_count += 1
        cycle_start = time.time()
        
        # Меняем значение для имитации джойстика
        if test_count % 10 == 0:
            channels[0] = 1000
        elif test_count % 10 == 5:
            channels[0] = 2000
        else:
            channels[0] = 1500
        
        try:
            request_start = time.time()
            response = requests.post(
                f"{api_url}/api/command/setChannels",
                json={"channels": channels},
                timeout=0.5
            )
            request_end = time.time()
            
            latency = (request_end - request_start) * 1000  # в мс
            latencies.append(latency)
            
            if response.status_code == 200:
                successes += 1
                status = "✓"
            else:
                failures += 1
                status = f"✗ HTTP {response.status_code}"
                
        except Exception as e:
            failures += 1
            status = f"✗ {str(e)[:30]}"
        
        # Вывод прогресса
        if test_count % 20 == 0:
            elapsed = time.time() - start_time
            print(f"[{elapsed:.1f}с] Отправок: {test_count}, "
                  f"Успешно: {successes}, Ошибок: {failures}, "
                  f"Последний: {status}")
        
        # Поддерживаем частоту
        cycle_time = time.time() - cycle_start
        sleep_time = max(0, interval - cycle_time)
        if sleep_time > 0:
            time.sleep(sleep_time)
    
    # Статистика
    print("\n" + "=" * 60)
    print("РЕЗУЛЬТАТЫ ТЕСТА:")
    print(f"Всего отправок: {test_count}")
    print(f"Успешно: {successes} ({successes/test_count*100:.1f}%)")
    print(f"Ошибок: {failures} ({failures/test_count*100:.1f}%)")
    
    if latencies:
        print(f"\nЗадержки (мс):")
        print(f"  Минимальная: {min(latencies):.1f}")
        print(f"  Максимальная: {max(latencies):.1f}")
        print(f"  Средняя: {statistics.mean(latencies):.1f}")
        print(f"  Медиана: {statistics.median(latencies):.1f}")
    
    print(f"\nФактическая частота: {test_count/duration:.1f} Гц")
    
    return successes > failures * 10  # Успехов должно быть в 10 раз больше ошибок

if __name__ == "__main__":
    import sys
    api_url = sys.argv[1] if len(sys.argv) > 1 else "http://192.168.1.101:8081"
    test_stability(api_url)