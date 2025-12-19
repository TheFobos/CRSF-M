# CRSF Python Bindings

Pybind11 модуль для работы с CRSF из Python.

## Структура

- `src/crsf_bindings.cpp` - Pybind11 модуль (C++ код)
- `crsf_wrapper.py` - Python обертка для удобной работы
- `build_lib.py` - Скрипт сборки модуля

## Сборка

1. Создать виртуальное окружение:
   ```bash
   python -m venv venv
   ```

2. Активировать виртуальное окружение (Linux):
   ```bash
   source venv/bin/activate
   ```

3. Установить зависимости:
   ```bash
   pip install -r requirements.txt
   ```

4. Собрать модуль:
   ```bash
   python build_lib.py build_ext --inplace
   ```

После сборки будет создан модуль `crsf_native.so` (или `.pyd` на Windows).

## Использование

### Через Python обертку (рекомендуется)

```python
from crsf_wrapper import CRSFWrapper

# Автоматическая инициализация (пытается получить указатель из основного приложения)
crsf = CRSFWrapper()
crsf.auto_init()

# Или инициализация с указателем вручную
import ctypes
# ... получение указателя на CRSF объект ...
crsf.init(crsf_ptr)

# Получение телеметрии
telemetry = crsf.get_telemetry()

# Установка режима работы
crsf.set_work_mode("joystick")  # или "manual"

# Установка канала
crsf.set_channel(1, 1500)  # канал 1, значение 1500

# Отправка каналов
crsf.send_channels()
```

### Напрямую через pybind11 модуль

```python
import crsf_native

# Инициализация
crsf_native.init_crsf_instance(crsf_ptr)

# Получение телеметрии
data = crsf_native.get_telemetry()

# Установка режима
crsf_native.set_work_mode("joystick")
```

## Примечания

- Основное приложение (`crsf_io_rpi`) должно быть запущено перед использованием Python обертки
- Для автоматической инициализации основное приложение должно быть скомпилировано как разделяемая библиотека или экспортировать функцию `crsfGetActive()`
- Если автоматическая инициализация не работает, используйте `init()` с указателем, полученным через ctypes

