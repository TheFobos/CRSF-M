# Python Bindings Guide

Подробное руководство по работе с CRSF через Python bindings (pybind11)

## Содержание

- [Установка и сборка](#установка-и-сборка)
- [Быстрый старт](#быстрый-старт)
- [Инициализация](#инициализация)
- [Получение телеметрии](#получение-телеметрии)
- [Управление каналами](#управление-каналами)
- [Режимы работы](#режимы-работы)
- [Примеры использования](#примеры-использования)
- [Обработка ошибок](#обработка-ошибок)
- [API Reference](#api-reference)

## Установка и сборка

### Требования

- Python 3.6+
- pybind11 >= 2.6.0
- Основное приложение `crsf_io_rpi` должно быть запущено

### Сборка модуля

```bash
cd pybind

# Установить зависимости
pip install -r requirements.txt

# Собрать модуль
python3 build_lib.py build_ext --inplace
```

После сборки будет создан модуль `crsf_native.so` (или `.pyd` на Windows).

## Быстрый старт

```python
from crsf_wrapper import CRSFWrapper

# Создать экземпляр и инициализировать
crsf = CRSFWrapper()
crsf.auto_init()

# Получить телеметрию
telemetry = crsf.get_telemetry()
print(f"Link: {'UP' if telemetry['linkUp'] else 'DOWN'}")
print(f"GPS: {telemetry['gps']['latitude']}, {telemetry['gps']['longitude']}")
print(f"Battery: {telemetry['battery']['voltage']}V")

# Переключиться в ручной режим
crsf.set_work_mode("manual")

# Установить канал
crsf.set_channel(1, 1500)
crsf.send_channels()
```

## Инициализация

### Автоматическая инициализация (рекомендуется)

```python
from crsf_wrapper import CRSFWrapper

crsf = CRSFWrapper()
crsf.auto_init()  # Автоматически находит запущенное приложение
```

Метод `auto_init()` проверяет наличие файла `/tmp/crsf_telemetry.dat`, который создается основным приложением. Если файл существует, инициализация считается успешной.

### Ручная инициализация

Если автоматическая инициализация не работает, можно использовать ручную:

```python
import ctypes

# Получить указатель на CRSF объект из основного приложения
# (требует специальной настройки основного приложения)
crsf_ptr = get_crsf_pointer()  # Ваша функция для получения указателя
crsf = CRSFWrapper(crsf_ptr)
```

## Получение телеметрии

### Полная телеметрия

```python
telemetry = crsf.get_telemetry()

# Структура данных:
{
    'linkUp': bool,              # Состояние связи
    'activePort': str,            # Активный порт
    'lastReceive': int,           # Время последнего приема
    'timestamp': str,             # Временная метка
    'channels': [int, ...],       # 16 каналов (1000-2000)
    'packetsReceived': int,       # Получено пакетов
    'packetsSent': int,           # Отправлено пакетов
    'packetsLost': int,           # Потеряно пакетов
    'gps': {
        'latitude': float,        # Широта
        'longitude': float,       # Долгота
        'altitude': float,        # Высота (м)
        'speed': float            # Скорость (км/ч)
    },
    'battery': {
        'voltage': float,         # Напряжение (В)
        'current': float,         # Ток (мА)
        'capacity': float,        # Емкость (мАч)
        'remaining': int          # Остаток (%)
    },
    'attitude': {
        'roll': float,            # Крен (градусы)
        'pitch': float,           # Тангаж (градусы)
        'yaw': float              # Рыскание (градусы, 0-360)
    },
    'attitudeRaw': {
        'roll': int,              # Сырое значение крена
        'pitch': int,             # Сырое значение тангажа
        'yaw': int                # Сырое значение рыскания
    },
    'workMode': str               # 'joystick' или 'manual'
}
```

### Примеры использования телеметрии

```python
# Проверка состояния связи
if telemetry['linkUp']:
    print("Связь установлена")
else:
    print("Связь потеряна")

# GPS координаты
gps = telemetry['gps']
print(f"Позиция: {gps['latitude']:.6f}, {gps['longitude']:.6f}")
print(f"Высота: {gps['altitude']:.1f} м")
print(f"Скорость: {gps['speed']:.1f} км/ч")

# Батарея
battery = telemetry['battery']
print(f"Напряжение: {battery['voltage']:.2f} В")
print(f"Ток: {battery['current']:.0f} мА")
print(f"Остаток: {battery['remaining']}%")

# Attitude
attitude = telemetry['attitude']
print(f"Крен: {attitude['roll']:.1f}°")
print(f"Тангаж: {attitude['pitch']:.1f}°")
print(f"Рыскание: {attitude['yaw']:.1f}°")

# Каналы
channels = telemetry['channels']
for i, value in enumerate(channels, 1):
    print(f"Канал {i}: {value}")
```

## Управление каналами

### Установка одного канала

```python
# Канал 1 (Roll) в центр
crsf.set_channel(1, 1500)

# Канал 2 (Pitch) минимум
crsf.set_channel(2, 1000)

# Канал 3 (Throttle) максимум
crsf.set_channel(3, 2000)

# Канал 4 (Yaw) влево
crsf.set_channel(4, 1300)

# Отправить пакет каналов
crsf.send_channels()
```

**Диапазон значений:** 1000-2000
- **1000** - минимум
- **1500** - центр
- **2000** - максимум

### Установка всех каналов одновременно

```python
# Установить все 16 каналов
channels = [
    1500,  # CH1: Roll - центр
    1500,  # CH2: Pitch - центр
    1000,  # CH3: Throttle - минимум
    1500,  # CH4: Yaw - центр
    1500,  # CH5
    1500,  # CH6
    1500,  # CH7
    1500,  # CH8
    1500,  # CH9
    1500,  # CH10
    1500,  # CH11
    1500,  # CH12
    1500,  # CH13
    1500,  # CH14
    1500,  # CH15
    1500   # CH16
]

crsf.set_channels(channels)
crsf.send_channels()
```

### RC Каналы

| Канал | Описание | Типичное использование |
|-------|----------|------------------------|
| 1 | Roll | Управление креном (влево/вправо) |
| 2 | Pitch | Управление тангажем (вперед/назад) |
| 3 | Throttle | Управление тягой (газ) |
| 4 | Yaw | Управление рысканием (поворот) |
| 5-16 | Aux | Вспомогательные каналы |

## Режимы работы

### Joystick режим

В этом режиме каналы управляются физическим джойстиком. Python не может устанавливать каналы.

```python
crsf.set_work_mode("joystick")
```

### Manual режим

В этом режиме каналы управляются программно через Python.

```python
# Переключиться в ручной режим
crsf.set_work_mode("manual")

# Теперь можно устанавливать каналы
crsf.set_channel(1, 1500)
crsf.send_channels()
```

### Проверка текущего режима

```python
mode = crsf.get_work_mode()
print(f"Текущий режим: {mode}")  # 'joystick' или 'manual'
```

## Примеры использования

### Пример 1: Мониторинг телеметрии

```python
from crsf_wrapper import CRSFWrapper
import time

crsf = CRSFWrapper()
crsf.auto_init()

while True:
    telemetry = crsf.get_telemetry()
    
    print(f"\n=== Телеметрия ===")
    print(f"Связь: {'UP' if telemetry['linkUp'] else 'DOWN'}")
    print(f"GPS: {telemetry['gps']['latitude']:.6f}, {telemetry['gps']['longitude']:.6f}")
    print(f"Батарея: {telemetry['battery']['voltage']:.2f}V ({telemetry['battery']['remaining']}%)")
    print(f"Attitude: R={telemetry['attitude']['roll']:.1f}° P={telemetry['attitude']['pitch']:.1f}° Y={telemetry['attitude']['yaw']:.1f}°")
    
    time.sleep(1)
```

### Пример 2: Управление каналами

```python
from crsf_wrapper import CRSFWrapper
import time

crsf = CRSFWrapper()
crsf.auto_init()

# Переключиться в ручной режим
crsf.set_work_mode("manual")

# Плавное изменение канала 1 от минимума к максимуму
for value in range(1000, 2001, 10):
    crsf.set_channel(1, value)
    crsf.send_channels()
    time.sleep(0.05)  # 50ms задержка

# Вернуть в центр
crsf.set_channel(1, 1500)
crsf.send_channels()
```

### Пример 3: Установка всех каналов

```python
from crsf_wrapper import CRSFWrapper

crsf = CRSFWrapper()
crsf.auto_init()
crsf.set_work_mode("manual")

# Установить все каналы в безопасные значения
safe_channels = [
    1500,  # Roll - центр
    1500,  # Pitch - центр
    1000,  # Throttle - минимум (безопасно)
    1500,  # Yaw - центр
    1500, 1500, 1500, 1500,  # Aux каналы
    1500, 1500, 1500, 1500,
    1500, 1500, 1500, 1500
]

crsf.set_channels(safe_channels)
crsf.send_channels()
```

### Пример 4: Автоматическое управление на основе телеметрии

```python
from crsf_wrapper import CRSFWrapper
import time

crsf = CRSFWrapper()
crsf.auto_init()
crsf.set_work_mode("manual")

while True:
    telemetry = crsf.get_telemetry()
    
    # Если батарея разряжена, установить throttle в минимум
    if telemetry['battery']['voltage'] < 10.0:
        crsf.set_channel(3, 1000)  # Throttle минимум
        crsf.send_channels()
        print("Низкое напряжение! Throttle установлен в минимум")
    
    # Если связь потеряна, установить все каналы в безопасные значения
    if not telemetry['linkUp']:
        crsf.set_channels([1500, 1500, 1000, 1500] + [1500] * 12)
        crsf.send_channels()
        print("Связь потеряна! Безопасный режим активирован")
    
    time.sleep(0.1)
```

## Тестирование и бенчмарки

### Бенчмарк задержки (benchmark_delay.py)

Утилита для измерения задержки отправки и получения каналов CRSF. Полезно для:
- Оценки производительности системы
- Диагностики проблем с задержкой
- Оптимизации производительности

**Использование:**
```bash
# Запуск с 5 тестами (по умолчанию)
python3 benchmark_delay.py

# Запуск с указанным количеством тестов
python3 benchmark_delay.py -n 10
python3 benchmark_delay.py --num-tests 20
```

**Что измеряется:**
Бенчмарк измеряет полную задержку от вызова `set_channel()` до получения обновленного значения в `get_telemetry()`, включая:
- Запись команды в файл `/tmp/crsf_command.txt`
- Чтение файла основным приложением
- Установку канала в CrsfSerial
- Отправку пакета каналов
- Запись телеметрии в файл `/tmp/crsf_telemetry.dat`
- Чтение файла Python оберткой

**Результаты:**
Бенчмарк выводит подробную статистику:
- Количество успешных тестов
- Минимальная, максимальная, средняя и медианная задержка

**Примечание:**
Перед запуском убедитесь, что основное приложение `crsf_io_rpi` запущено. Бенчмарк автоматически переключается в режим `manual` для тестирования.

Подробнее: [python_README.md](python_README.md#2-бенчмарк-задержки-benchmark_delaypy)

## Обработка ошибок

### Проверка инициализации

```python
from crsf_wrapper import CRSFWrapper

crsf = CRSFWrapper()

try:
    crsf.auto_init()
except RuntimeError as e:
    print(f"Ошибка инициализации: {e}")
    print("Убедитесь, что crsf_io_rpi запущен")
    exit(1)

# Проверка статуса
if not crsf.is_initialized:
    print("CRSF не инициализирован")
    exit(1)
```

### Валидация параметров

```python
try:
    # Неверный номер канала
    crsf.set_channel(0, 1500)  # ValueError: Номер канала должен быть от 1 до 16
except ValueError as e:
    print(f"Ошибка: {e}")

try:
    # Неверное значение канала
    crsf.set_channel(1, 500)  # ValueError: Значение канала должно быть от 1000 до 2000
except ValueError as e:
    print(f"Ошибка: {e}")

try:
    # Неверный режим
    crsf.set_work_mode("invalid")  # ValueError: Неверный режим
except ValueError as e:
    print(f"Ошибка: {e}")
```

### Обработка ошибок получения телеметрии

```python
try:
    telemetry = crsf.get_telemetry()
except RuntimeError as e:
    print(f"Ошибка получения телеметрии: {e}")
    # Возможно, приложение не запущено или связь потеряна
```

## API Reference

### CRSFWrapper

#### Методы

##### `__init__(crsf_ptr=None)`
Создает экземпляр обертки.

**Параметры:**
- `crsf_ptr` (optional): Указатель на C++ CRSF объект

##### `auto_init()`
Автоматическая инициализация. Проверяет наличие файла `/tmp/crsf_telemetry.dat`.

**Исключения:**
- `RuntimeError`: Если файл телеметрии не найден

##### `init(crsf_ptr)`
Ручная инициализация с указателем.

**Параметры:**
- `crsf_ptr`: Указатель на CRSF объект (int, ctypes.c_void_p)

##### `get_telemetry() -> Dict`
Получить полную телеметрию.

**Возвращает:**
- `Dict`: Словарь с данными телеметрии

**Исключения:**
- `RuntimeError`: Если CRSF не инициализирован

##### `set_work_mode(mode: str)`
Установить режим работы.

**Параметры:**
- `mode`: `'joystick'` или `'manual'`

**Исключения:**
- `ValueError`: Если режим неверный

##### `get_work_mode() -> str`
Получить текущий режим работы.

**Возвращает:**
- `str`: `'joystick'` или `'manual'`

##### `set_channel(channel: int, value: int)`
Установить значение канала.

**Параметры:**
- `channel`: Номер канала (1-16)
- `value`: Значение (1000-2000)

**Исключения:**
- `ValueError`: Если параметры вне диапазона

##### `set_channels(channels: List[int])`
Установить все каналы одновременно.

**Параметры:**
- `channels`: Список из 16 значений (1000-2000)

**Исключения:**
- `ValueError`: Если список не содержит 16 элементов

##### `send_channels()`
Отправить пакет каналов.

#### Свойства

##### `is_initialized -> bool`
Проверка инициализации.

## Примечания

- Основное приложение (`crsf_io_rpi`) должно быть запущено перед использованием Python обертки
- Данные телеметрии читаются из файла `/tmp/crsf_telemetry.dat` безопасным способом
- В режиме `joystick` установка каналов через Python не работает
- В режиме `manual` каналы управляются только через Python
- Частота отправки каналов: ~100 Гц (10ms период)
- Все значения каналов автоматически ограничиваются диапазоном 1000-2000

## Устранение неполадок

### Ошибка: "Файл с телеметрией не найден"

**Причина:** Основное приложение не запущено или не создает файл телеметрии.

**Решение:**
1. Убедитесь, что `crsf_io_rpi` запущен: `ps aux | grep crsf_io_rpi`
2. Проверьте наличие файла: `ls -l /tmp/crsf_telemetry.dat`
3. Перезапустите основное приложение: `sudo ./crsf_io_rpi`

### Ошибка: "CRSF не инициализирован"

**Причина:** Метод `auto_init()` не был вызван или завершился с ошибкой.

**Решение:**
```python
try:
    crsf.auto_init()
except RuntimeError as e:
    print(f"Ошибка: {e}")
```

### Каналы не изменяются

**Причина:** Режим работы установлен в `joystick`.

**Решение:**
```python
crsf.set_work_mode("manual")
```

### Модуль не найден

**Причина:** Модуль не собран или путь неверный.

**Решение:**
```bash
cd pybind
python3 build_lib.py build_ext --inplace
```

---

**Версия:** 4.3

