# CRSF-IO-mkII

Raspberry Pi CRSF Protocol Interface для управления коптерами

## Описание

CRSF-IO-mkII - это система для Raspberry Pi, которая обеспечивает прием и отправку CRSF (Crossfire) протокола, управление через джойстик или Python bindings, веб-интерфейс для телеметрии и поддержку различных режимов работы.

## Возможности

- ✅ Прием RC-каналов от полетного контроллера
- ✅ Отправка команд управления
- ✅ Телеметрия в реальном времени (GPS, Батарея, Attitude)
- ✅ Python GUI интерфейс
- ✅ Python bindings (pybind11) для интеграции с Python приложениями
- ✅ HTTP API сервер для удаленного управления (ведущий узел)
- ✅ HTTP API интерпретатор для приема команд (ведомый узел)
- ✅ Телеметрия через API в реальном времени
- ✅ Поддержка двух режимов работы (Joystick/Manual)
- ✅ Режим работы без телеметрии (--notel)
- ✅ Fail-safe защита
- ✅ Полное покрытие unit-тестами (86 тестов)

## Быстрый старт

### Установка

```bash
git clone <repository_url>
cd CRSF-IO-mkII
make
```

### Запуск

#### Локальный режим (без API)
```bash
sudo ./crsf_io_rpi
```

#### Режим без телеметрии (для тестирования)
```bash
sudo ./crsf_io_rpi --notel
```

#### Режим с API (удаленное управление)
См. [API_START_GUIDE.md](API_START_GUIDE.md) для подробных инструкций.

### Python GUI

```bash
python3 crsf_realtime_interface.py
```

### Python Bindings (pybind11)

```bash
cd pybind
python3 build_lib.py build_ext --inplace
python3 -c "from crsf_wrapper import CRSFWrapper; crsf = CRSFWrapper(); crsf.auto_init()"
```

Подробнее: [pybind/README.md](pybind/README.md)

## Документация

### Быстрые гайды
- **[Quick Start Guide](QUICK_START.md)** - Быстрый запуск передачи по API
- **[API Start Guide](API_START_GUIDE.md)** - Инструкция по запуску API системы
- **[API Telemetry Guide](API_TELEMETRY_GUIDE.md)** - Руководство по использованию API с телеметрией
- **[Receiver Node Instructions](RECEIVER_NODE_INSTRUCTIONS.md)** - Инструкции для принимающего узла

### Подробная документация
- **[Python Bindings Guide](docs/PYTHON_BINDINGS_README.md)** - Подробное руководство по работе с pybind11
- **[API Server Guide](docs/API_SERVER_README.md)** - Документация по API серверу и интерпретатору
- **[Configuration Guide](docs/CONFIG_README.md)** - Настройка и конфигурация
- **[Build Guide](docs/MAKEFILE_README.md)** - Руководство по сборке
- **[Manual Mode Guide](docs/MANUAL_MODE_GUIDE.md)** - Ручной режим управления через Python
- **[Telemetry Documentation](docs/README_telemetry.md)** - Документация по телеметрии
- **[Python Bindings](pybind/README.md)** - Pybind11 модуль для Python
- **[Unit Tests](unit/README.md)** - Документация по unit-тестам
- **[Documentation Index](docs/DOCUMENTATION_INDEX.md)** - Полный список документации

## Работа с Python

### Python Bindings (локальное управление)

```python
from crsf_wrapper import CRSFWrapper

crsf = CRSFWrapper()
crsf.auto_init()

# Получить все данные телеметрии
telemetry = crsf.get_telemetry()
print(f"GPS: {telemetry['gps']['latitude']}, {telemetry['gps']['longitude']}")
print(f"Battery: {telemetry['battery']['voltage']}V")
print(f"Attitude: Roll={telemetry['attitude']['roll']}°")

# Переключиться в ручной режим
crsf.set_work_mode("manual")

# Установить канал 1 в центр (1500)
crsf.set_channel(1, 1500)
crsf.send_channels()
```

Подробнее: [PYTHON_BINDINGS_README.md](docs/PYTHON_BINDINGS_README.md)

### Python API Wrapper (удаленное управление)

```python
from api_wrapper import CRSFAPIWrapper

# Подключение к API серверу на ведущем узле
crsf = CRSFAPIWrapper("http://localhost:8081")
crsf.auto_init()

# Получить телеметрию через API
telemetry = crsf.get_telemetry()

# Управление каналами через API
crsf.set_work_mode("manual")
crsf.set_channel(1, 1500)
crsf.send_channels()
```

Подробнее: [API_START_GUIDE.md](API_START_GUIDE.md)

## Тестирование

Проект включает полный набор unit-тестов (86 тестов), покрывающих все основные компоненты:

```bash
# Запуск всех тестов
cd unit
bash run_tests.sh

# Или через make
cd unit && make test
```

**Статус тестов:** ✅ Все 86 тестов успешно пройдены

Тесты покрывают:
- CRC8 вычисления и валидация
- Парсинг всех типов CRSF пакетов (GPS, Battery, Link Statistics, Attitude, Flight Mode)
- Кодирование/декодирование каналов
- Управление буфером и обработка ошибок
- Состояние связи и fail-safe механизм
- Отправка пакетов и телеметрия

Подробнее: [unit/README.md](unit/README.md)

## Структура проекта

```
CRSF-IO-mkII/
├── main.cpp                    # Главная точка входа
├── config.h                    # Конфигурация (см. docs/CONFIG_README.md)
├── Makefile                    # Сборка (см. docs/MAKEFILE_README.md)
├── api_server.cpp              # HTTP API сервер (ведущий узел)
├── api_interpreter.cpp         # HTTP API интерпретатор (ведомый узел)
├── api_wrapper.py              # Python обертка для API
├── crsf/                       # CRSF модуль
├── libs/                       # Библиотеки
├── pybind/                     # Python bindings (pybind11)
├── unit/                       # Unit-тесты (86 тестов)
├── telemetry_server.cpp        # Веб-сервер
├── crsf_realtime_interface.py  # Python GUI
├── start_*.sh                  # Скрипты запуска
├── stop_*.sh                   # Скрипты остановки
└── docs/                       # Документация
```

## Требования

- Raspberry Pi (протестировано на Raspberry Pi 5)
- Linux с поддержкой GPIO и UART
- C++17 компилятор
- Python 3.x
- Google Test и Google Mock (для unit-тестов)

## Лицензия

(Укажите вашу лицензию)

## Поддержка

При возникновении проблем создайте issue в репозитории.
