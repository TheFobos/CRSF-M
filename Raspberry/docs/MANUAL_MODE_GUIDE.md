# Manual Mode Guide

Руководство по использованию ручного режима управления через Python bindings

## Что такое Manual Mode

Ручной режим позволяет программно управлять RC-каналами через Python, минуя джойстик. Это полезно для:
- Автоматизации полетов
- Тестирования
- Интеграции с другими системами
- Сценариев управления

## Установка и инициализация

### Установка модуля

```bash
cd pybind
pip install -r requirements.txt
python3 build_lib.py build_ext --inplace
```

### Инициализация

```python
from crsf_wrapper import CRSFWrapper

# Создать экземпляр и инициализировать
crsf = CRSFWrapper()
crsf.auto_init()
```

## Переключение режимов

### Режим джойстика (по умолчанию)

```python
crsf.set_work_mode("joystick")
```

В этом режиме каналы управляются джойстиком. Python не может устанавливать каналы.

### Ручной режим

```python
crsf.set_work_mode("manual")
```

В этом режиме каналы управляются через Python.

### Проверка текущего режима

```python
mode = crsf.get_work_mode()
print(f"Текущий режим: {mode}")  # 'joystick' или 'manual'
```

## Управление каналами

### RC Каналы

| Канал | Описание | Диапазон | Центр |
|-------|----------|----------|-------|
| 1 | Roll | 1000-2000 | 1500 |
| 2 | Pitch | 1000-2000 | 1500 |
| 3 | Throttle | 1000-2000 | 1500 |
| 4 | Yaw | 1000-2000 | 1500 |
| 5-16 | Aux | 1000-2000 | 1500 |

**Значения:**
- `1000` - Минимум/Влево/Вниз
- `1500` - Центр/Нейтраль
- `2000` - Максимум/Вправо/Вверх

### Установка одного канала

```python
# Канал 1 (Roll) в центр
crsf.set_channel(1, 1500)
crsf.send_channels()

# Канал 2 (Pitch) минимум
crsf.set_channel(2, 1000)
crsf.send_channels()

# Канал 3 (Throttle) максимум
crsf.set_channel(3, 2000)
crsf.send_channels()
```

### Установка всех каналов одновременно

```python
# Установить все 16 каналов
channels = [
    1500,  # CH1: Roll - центр
    1500,  # CH2: Pitch - центр
    1000,  # CH3: Throttle - минимум
    1500,  # CH4: Yaw - центр
    1500, 1500, 1500, 1500,  # CH5-8
    1500, 1500, 1500, 1500,  # CH9-12
    1500, 1500, 1500, 1500   # CH13-16
]

crsf.set_channels(channels)
crsf.send_channels()
```

## Примеры

### Установка канала в центр

```python
crsf.set_channel(1, 1500)
crsf.send_channels()
```

### Roll влево

```python
crsf.set_channel(1, 1200)
crsf.send_channels()
```

### Roll вправо

```python
crsf.set_channel(1, 1800)
crsf.send_channels()
```

### Pitch вниз

```python
crsf.set_channel(2, 1200)
crsf.send_channels()
```

### Pitch вверх

```python
crsf.set_channel(2, 1800)
crsf.send_channels()
```

### Throttle минимум

```python
crsf.set_channel(3, 1000)
crsf.send_channels()
```

### Throttle максимум

```python
crsf.set_channel(3, 2000)
crsf.send_channels()
```

### Yaw влево

```python
crsf.set_channel(4, 1300)
crsf.send_channels()
```

### Yaw вправо

```python
crsf.set_channel(4, 1700)
crsf.send_channels()
```

## Типичные сценарии

### 1. Возврат всех каналов в центр

```python
from crsf_wrapper import CRSFWrapper

crsf = CRSFWrapper()
crsf.auto_init()
crsf.set_work_mode("manual")

# Установить безопасные значения
crsf.set_channels([1500, 1500, 1000, 1500] + [1500] * 12)
crsf.send_channels()
```

### 2. Последовательность движений

```python
from crsf_wrapper import CRSFWrapper
import time

crsf = CRSFWrapper()
crsf.auto_init()
crsf.set_work_mode("manual")

# Право
crsf.set_channel(1, 1800)
crsf.send_channels()
time.sleep(2)

# Центр
crsf.set_channel(1, 1500)
crsf.send_channels()
time.sleep(1)

# Влево
crsf.set_channel(1, 1200)
crsf.send_channels()
time.sleep(2)

# Центр
crsf.set_channel(1, 1500)
crsf.send_channels()
```

### 3. Плавное изменение канала

```python
from crsf_wrapper import CRSFWrapper
import time

crsf = CRSFWrapper()
crsf.auto_init()
crsf.set_work_mode("manual")

# Плавное изменение Roll от минимума к максимуму
for value in range(1000, 2001, 10):
    crsf.set_channel(1, value)
    crsf.send_channels()
    time.sleep(0.05)  # 50ms задержка

# Вернуть в центр
crsf.set_channel(1, 1500)
crsf.send_channels()
```

### 4. Автоматическое управление на основе телеметрии

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

### 5. Тестирование всех каналов

```python
from crsf_wrapper import CRSFWrapper
import time

crsf = CRSFWrapper()
crsf.auto_init()
crsf.set_work_mode("manual")

# Установить все каналы в центр
crsf.set_channels([1500] * 16)
crsf.send_channels()
time.sleep(1)

# Тест каждого канала по очереди
for channel in range(1, 17):
    print(f"Тест канала {channel}")
    
    # Минимум
    crsf.set_channel(channel, 1000)
    crsf.send_channels()
    time.sleep(0.5)
    
    # Центр
    crsf.set_channel(channel, 1500)
    crsf.send_channels()
    time.sleep(0.5)
    
    # Максимум
    crsf.set_channel(channel, 2000)
    crsf.send_channels()
    time.sleep(0.5)
    
    # Вернуть в центр
    crsf.set_channel(channel, 1500)
    crsf.send_channels()
    time.sleep(0.5)

# Вернуться к джойстику
crsf.set_work_mode("joystick")
```

### 6. Измерение производительности (бенчмарк)

Для измерения задержки отправки и получения каналов используйте утилиту `benchmark_delay.py`:

```bash
# Запуск с 5 тестами (по умолчанию)
python3 benchmark_delay.py

# Запуск с указанным количеством тестов
python3 benchmark_delay.py -n 10
python3 benchmark_delay.py --num-tests 20
```

**Что измеряется:**
- Полная задержка от `set_channel()` до получения обновленного значения в `get_telemetry()`
- Включает все этапы: запись команды, обработку основным приложением, отправку пакета, запись телеметрии, чтение Python оберткой

**Результаты:**
Бенчмарк выводит статистику:
- Количество успешных тестов
- Минимальная, максимальная, средняя и медианная задержка

**Пример использования:**
```bash
# Быстрый тест (5 тестов)
python3 benchmark_delay.py

# Подробный тест (20 тестов для более точной статистики)
python3 benchmark_delay.py -n 20
```

**Примечание:**
Бенчмарк автоматически переключается в режим `manual` для тестирования. Убедитесь, что основное приложение `crsf_io_rpi` запущено перед запуском бенчмарка.

Подробнее: [python_README.md](python_README.md#2-бенчмарк-задержки-benchmark_delaypy)

## Безопасность

### ⚠️ ВАЖНО

1. **Всегда устанавливайте безопасные значения перед переключением в ручной режим**
2. **Throttle должен быть на минимуме (1000) при наземных операциях**
3. **Всегда возвращайтесь в режим джойстика после тестирования**

### Рекомендуемая последовательность

```python
from crsf_wrapper import CRSFWrapper

crsf = CRSFWrapper()
crsf.auto_init()

# 1. Установить безопасные значения
crsf.set_channels([1500, 1500, 1000, 1500] + [1500] * 12)
crsf.send_channels()

# 2. Переключиться в ручной режим
crsf.set_work_mode("manual")

# 3. Выполнить операции
# ... ваш код ...

# 4. Вернуться в режим джойстика
crsf.set_work_mode("joystick")
```

## Проверка текущего режима

```python
telemetry = crsf.get_telemetry()
print(f"Режим работы: {telemetry['workMode']}")  # 'joystick' или 'manual'
```

Или напрямую:

```python
mode = crsf.get_work_mode()
print(f"Текущий режим: {mode}")
```

## Частота обновления

- Значения каналов устанавливаются мгновенно
- Отправка CRSF каналов: 100 Гц (каждые 10 мс)
- Рекомендуется вызывать `send_channels()` после каждого изменения

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
```

### Валидация параметров

```python
try:
    # Неверный номер канала
    crsf.set_channel(0, 1500)  # ValueError
except ValueError as e:
    print(f"Ошибка: {e}")

try:
    # Неверное значение канала
    crsf.set_channel(1, 500)  # ValueError
except ValueError as e:
    print(f"Ошибка: {e}")
```

## Ограничения

- Ручной режим работает только через Python bindings
- Джойстик не реагирует в ручном режиме
- Значения сохраняются до следующего изменения
- При потере связи fail-safe срабатывает автоматически
- Основное приложение (`crsf_io_rpi`) должно быть запущено

## Дополнительные ресурсы

- [Python Bindings Guide](PYTHON_BINDINGS_README.md) - Подробное руководство по pybind11
- [pybind/README.md](../pybind/README.md) - Базовая документация модуля

---

**Версия:** 4.3
