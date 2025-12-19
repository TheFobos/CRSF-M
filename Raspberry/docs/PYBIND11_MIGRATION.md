# Подробное описание миграции с HTTP API на pybind11

## Содержание
1. [Обзор изменений](#обзор-изменений)
2. [Архитектура решения](#архитектура-решения)
3. [Создание pybind11 модуля](#создание-pybind11-модуля)
4. [Создание Python обертки](#создание-python-обертки)
5. [Проблемы и решения](#проблемы-и-решения)
6. [Детали реализации](#детали-реализации)

---

## Обзор изменений

### Что было до изменений:
- **HTTP API сервер** (`telemetry_server.cpp/h`) - веб-сервер на порту 8081
- **JSON API** - Python интерфейс получал данные через HTTP запросы (`requests.get()`)
- **Управление через URL** - команды отправлялись через HTTP GET запросы
- **Отдельные процессы** - основное приложение и Python интерфейс работали независимо

### Что стало после изменений:
- **pybind11 модуль** (`crsf_native.so`) - C++ библиотека, доступная из Python
- **Python обертка** (`crsf_wrapper.py`) - удобный интерфейс для работы с pybind11
- **Файловое межпроцессное взаимодействие** - данные передаются через файлы `/tmp/crsf_telemetry.dat` и `/tmp/crsf_command.txt`
- **Прямой доступ к данным** - без HTTP запросов, данные читаются напрямую из файлов

---

## Архитектура решения

### Схема работы:

```
┌─────────────────────────────────────────────────────────────┐
│  Основное приложение (crsf_io_rpi)                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  CRSF обработка (crsf/crsf.cpp)                       │  │
│  │  - Получение данных с полетника                        │  │
│  │  - Обработка пакетов                                   │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                   │
│                          ▼                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Поток записи телеметрии (main.cpp)                   │  │
│  │  - Читает данные из CrsfSerial                        │  │
│  │  - Записывает в /tmp/crsf_telemetry.dat               │  │
│  │  - Обновление каждые 20мс                             │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                   │
│                          ▼                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Обработка команд (main.cpp)                          │  │
│  │  - Читает /tmp/crsf_command.txt                       │  │
│  │  - Выполняет команды (setChannel, setMode)            │  │
│  │  - Удаляет файл после обработки                        │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ Файлы
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Python интерфейс (crsf_realtime_interface.py)              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Python обертка (crsf_wrapper.py)                    │  │
│  │  - CRSFWrapper класс                                  │  │
│  │  - Методы: get_telemetry(), set_channel() и т.д.      │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                   │
│                          ▼                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  pybind11 модуль (crsf_native.so)                     │  │
│  │  - C++ функции, обернутые для Python                  │  │
│  │  - Чтение /tmp/crsf_telemetry.dat                     │  │
│  │  - Запись /tmp/crsf_command.txt                       │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Создание pybind11 модуля

### Файл: `pybind/src/crsf_bindings.cpp`

#### Структура данных телеметрии

**Проблема:** Нужно было передать сложную структуру данных из C++ в Python.

**Решение:** Создана структура `TelemetryData` с полями для всех данных телеметрии:

```cpp
struct TelemetryData {
    bool linkUp = false;
    std::string activePort = "Unknown";
    uint32_t lastReceive = 0;
    std::vector<int> channels;  // 16 каналов
    uint32_t packetsReceived = 0;
    uint32_t packetsSent = 0;
    uint32_t packetsLost = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double speed = 0.0;
    double voltage = 0.0;
    double current = 0.0;
    double capacity = 0.0;
    uint8_t remaining = 0;
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    int16_t rollRaw = 0;
    int16_t pitchRaw = 0;
    int16_t yawRaw = 0;
    std::string timestamp;
};
```

**Почему так:**
- `std::vector<int>` автоматически конвертируется в Python `list` благодаря `#include <pybind11/stl.h>`
- `std::string` автоматически конвертируется в Python `str`
- Все числовые типы конвертируются автоматически

#### Экспорт структуры в Python

```cpp
py::class_<TelemetryData>(m, "TelemetryData")
    .def_readwrite("linkUp", &TelemetryData::linkUp)
    .def_readwrite("channels", &TelemetryData::channels)
    // ... и так далее для всех полей
```

**Как это работает:**
- `py::class_<TelemetryData>` создает Python класс, соответствующий C++ структуре
- `.def_readwrite()` делает поля доступными для чтения и записи из Python
- pybind11 автоматически генерирует код конвертации типов

#### Функция получения телеметрии

**Первоначальная проблема:** Прямой доступ к указателю `CrsfSerial*` из другого процесса вызывал segmentation fault.

**Первая попытка решения (неудачная):**
```cpp
void initCrsfInstance(uintptr_t crsfPtrValue) {
    crsfInstance = reinterpret_cast<CrsfSerial*>(crsfPtrValue);
}
```

**Почему не работало:**
- Указатели из одного процесса недействительны в другом процессе
- Каждый процесс имеет свое виртуальное адресное пространство
- Попытка использовать указатель из другого процесса = segfault

**Финальное решение:** Чтение данных из файла

```cpp
TelemetryData getTelemetry() {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    TelemetryData data;
    
    // Читаем данные из файла, созданного основным приложением
    std::ifstream file("/tmp/crsf_telemetry.dat", std::ios::binary);
    if (file.is_open() && file.good()) {
        SharedTelemetryData shared;
        file.read(reinterpret_cast<char*>(&shared), sizeof(SharedTelemetryData));
        file.close();
        
        if (file.gcount() == sizeof(SharedTelemetryData)) {
            // Копируем данные из разделяемой структуры
            data.linkUp = shared.linkUp;
            data.channels.assign(shared.channels, shared.channels + 16);
            // ... и так далее
        }
    }
    
    return data;
}
```

**Почему это работает:**
- Файл - это безопасный способ передачи данных между процессами
- Бинарный формат обеспечивает быструю запись/чтение
- Нет проблем с указателями - передаются только значения

#### Структура для файлового обмена

```cpp
struct SharedTelemetryData {
    bool linkUp;
    uint32_t lastReceive;
    int channels[16];  // Массив вместо vector для бинарной совместимости
    // ... остальные поля
};
```

**Важно:** Используется массив `int channels[16]` вместо `std::vector<int>`, потому что:
- `std::vector` содержит указатели внутри, размер структуры не фиксирован
- Массив имеет фиксированный размер, что позволяет безопасно записывать/читать бинарно
- `sizeof(SharedTelemetryData)` всегда одинаковый

---

## Создание Python обертки

### Файл: `pybind/crsf_wrapper.py`

#### Класс CRSFWrapper

**Цель:** Создать удобный Python интерфейс, скрывающий детали работы с pybind11.

```python
class CRSFWrapper:
    def __init__(self, crsf_ptr=None):
        self._initialized = False
        if crsf_ptr is not None:
            self.init(crsf_ptr)
```

**Почему нужна обертка:**
- Прямая работа с pybind11 модулем неудобна
- Нужна валидация данных
- Нужно преобразование форматов данных
- Удобнее работать с объектом, чем с модулем

#### Автоматическая инициализация

**Проблема:** Как узнать, что основное приложение запущено и готово к работе?

**Решение:** Проверка наличия файла с телеметрией

```python
def auto_init(self):
    telemetry_file = "/tmp/crsf_telemetry.dat"
    if os.path.exists(telemetry_file):
        self._initialized = True
        return
    raise RuntimeError("Файл с телеметрией не найден...")
```

**Почему просто проверка файла:**
- Если файл существует, значит основное приложение запущено
- Не нужно передавать указатели между процессами
- Просто и надежно

#### Получение телеметрии

```python
def get_telemetry(self) -> Dict:
    if not self._initialized:
        raise RuntimeError("CRSF не инициализирован...")
    
    data = crsf_native.get_telemetry()
    
    # Преобразуем в формат, совместимый с API
    return {
        'linkUp': data.linkUp,
        'channels': data.channels,
        'gps': {
            'latitude': data.latitude,
            # ...
        },
        # ...
    }
```

**Почему преобразование формата:**
- Старый API возвращал вложенные словари (`gps`, `battery`, `attitude`)
- pybind11 возвращает плоскую структуру
- Преобразование обеспечивает совместимость с существующим кодом интерфейса

#### Установка каналов

**Проблема:** Нужно установить все 16 каналов одновременно, но они устанавливались по одному.

**Первая версия (неудачная):**
```python
def apply_all_channels(self):
    for ch_num in range(16):
        self.send_channel_command(ch_num + 1, value)
        time.sleep(0.01)  # Задержка между командами
```

**Проблемы:**
- Каждая команда записывалась в отдельный файл
- Файл обрабатывался только один раз за цикл
- Команды перезаписывали друг друга
- Нужно было нажимать кнопку 14-25 раз

**Решение:** Функция для установки всех каналов одной командой

```python
def set_channels(self, channels: List[int]):
    if len(channels) < 16:
        raise ValueError(f"Должно быть 16 каналов...")
    crsf_native.set_channels(channels[:16])
```

**В C++:**
```cpp
void setChannels(const std::vector<int>& channels) {
    if (channels.size() >= 16) {
        std::ofstream cmdFile("/tmp/crsf_command.txt");
        if (cmdFile.is_open()) {
            cmdFile << "setChannels";
            for (size_t i = 0; i < 16; i++) {
                if (channels[i] >= 1000 && channels[i] <= 2000) {
                    cmdFile << " " << (i + 1) << "=" << channels[i];
                }
            }
            cmdFile << std::endl;
            cmdFile.close();
        }
    }
}
```

**Формат команды:** `setChannels 1=1500 2=1600 3=1700 ... 16=2000`

**Обработка в main.cpp:**
```cpp
if (cmd.find("setChannels") == 0) {
    std::istringstream iss(cmd);
    std::string token;
    iss >> token; // пропускаем "setChannels"
    while (iss >> token) {
        size_t pos = token.find('=');
        if (pos != std::string::npos) {
            unsigned int ch = std::stoi(token.substr(0, pos));
            int value = std::stoi(token.substr(pos + 1));
            if (ch >= 1 && ch <= 16 && value >= 1000 && value <= 2000) {
                crsfSetChannel(ch, value);
            }
        }
    }
}
```

**Почему это работает:**
- Все каналы устанавливаются в одном цикле обработки
- Одна команда вместо 16 отдельных
- Нет конфликтов при записи в файл

---

## Проблемы и решения

### Проблема 1: Segmentation Fault при использовании указателей

**Симптомы:**
- Программа падала с `Segmentation fault` при попытке получить телеметрию
- Ошибка возникала при обращении к `crsfInstance->isLinkUp()`

**Причина:**
- Указатель `CrsfSerial*` был получен из другого процесса (crsf_io_rpi)
- Указатели из одного процесса недействительны в другом процессе
- Виртуальная память процессов изолирована

**Попытки решения:**

1. **Попытка 1: Передача указателя через файл**
   ```cpp
   // main.cpp
   void* crsfPtr = crsfGetActive();
   ptrFile << reinterpret_cast<uintptr_t>(crsfPtr) << std::endl;
   
   // pybind
   uintptr_t ptr_value = int(ptr_str);
   crsfInstance = reinterpret_cast<CrsfSerial*>(ptr_value);
   ```
   **Результат:** Segfault - указатель недействителен в другом процессе

2. **Попытка 2: Загрузка через ctypes**
   ```python
   lib = ctypes.CDLL('./crsf_io_rpi', ctypes.RTLD_GLOBAL)
   crsf_get_active = lib.crsfGetActive
   crsf_ptr = crsf_get_active()
   ```
   **Результат:** `OSError: cannot dynamically load position-independent executable`
   - Исполняемый файл нельзя загрузить как библиотеку

3. **Финальное решение: Файловое межпроцессное взаимодействие**
   ```cpp
   // main.cpp - запись данных
   std::ofstream file("/tmp/crsf_telemetry.dat", std::ios::binary);
   file.write(reinterpret_cast<const char*>(&shared), sizeof(SharedTelemetryData));
   
   // pybind - чтение данных
   std::ifstream file("/tmp/crsf_telemetry.dat", std::ios::binary);
   file.read(reinterpret_cast<char*>(&shared), sizeof(SharedTelemetryData));
   ```
   **Результат:** Работает! Нет проблем с указателями

### Проблема 2: Каналы устанавливались не все одновременно

**Симптомы:**
- При нажатии "Применить все" устанавливалось только несколько каналов
- Нужно было нажимать кнопку 14-25 раз
- При каждом нажатии менялось разное количество каналов

**Причина:**
- Каждая команда `setChannel` записывалась в отдельный файл
- Файл обрабатывался только один раз за цикл в main.cpp
- Команды перезаписывали друг друга
- Файл удалялся после обработки первой команды

**Код до исправления:**
```python
def apply_all_channels(self):
    for ch_num in range(16):
        self.send_channel_command(ch_num + 1, value)
        time.sleep(0.01)
```

```cpp
// main.cpp - обрабатывалась только первая строка
std::getline(cmdFile, cmd);  // Только одна команда!
if (cmd.find("setChannel") == 0) {
    // обработка
}
remove("/tmp/crsf_command.txt");  // Файл удаляется сразу
```

**Решение:**

1. **Функция для установки всех каналов:**
   ```cpp
   void setChannels(const std::vector<int>& channels) {
       std::ofstream cmdFile("/tmp/crsf_command.txt");
       cmdFile << "setChannels";
       for (size_t i = 0; i < 16; i++) {
           cmdFile << " " << (i + 1) << "=" << channels[i];
       }
       cmdFile << std::endl;
   }
   ```

2. **Обработка всех команд из файла:**
   ```cpp
   // main.cpp - теперь обрабатываются все строки
   while (std::getline(cmdFile, cmd)) {  // Цикл по всем строкам!
       if (cmd.find("setChannels") == 0) {
           // Парсинг и установка всех каналов
       }
   }
   remove("/tmp/crsf_command.txt");  // Удаляется только после обработки всех
   ```

3. **Использование в Python:**
   ```python
   def apply_all_channels(self):
       channels = []
       for ch_num in range(16):
           channels.append(int(var.get()))
       self.crsf.set_channels(channels)  # Одна команда для всех!
   ```

**Результат:** Все 16 каналов устанавливаются одновременно одним нажатием кнопки

### Проблема 3: Режим по умолчанию

**Требование:** Сделать ручной режим управления режимом по умолчанию.

**Изменения:**

1. **main.cpp:**
   ```cpp
   std::string getWorkMode() {
       return "manual";  // Было: "joystick"
   }
   ```

2. **pybind/src/crsf_bindings.cpp:**
   ```cpp
   static std::string workMode = "manual";  // Было: "joystick"
   ```

3. **crsf_realtime_interface.py:**
   ```python
   self.mode_var = tk.StringVar(value="manual")  // Было: "joystick"
   ```

**Результат:** По умолчанию установлен ручной режим управления

### Проблема 4: Компиляция с C++14 vs C++17

**Проблема:** Ошибка компиляции `'std::filesystem' has not been declared`

**Причина:**
- `libs/rpi_hal.cpp` использует `<filesystem>`, который требует C++17
- `build_lib.py` был настроен на C++14

**Решение:**
```python
# build_lib.py
compile_args = ['-std=c++17', '-O2']  # Было: '-std=c++14'
```

**Почему C++17:**
- `<filesystem>` доступен только с C++17
- pybind11 поддерживает C++17
- Современный стандарт обеспечивает лучшую поддержку

---

## Детали реализации

### Запись телеметрии в main.cpp

```cpp
// Структура для записи (должна совпадать с pybind!)
struct SharedTelemetryData {
    bool linkUp;
    uint32_t lastReceive;
    int channels[16];  // Массив, не vector!
    // ... остальные поля
};

// Поток для записи телеметрии
std::thread telemetryWriterThread([&]() {
    CrsfSerial* crsf = static_cast<CrsfSerial*>(crsfGetActive());
    if (crsf == nullptr) return;
    
    while (true) {
        SharedTelemetryData shared;
        
        // Читаем данные из CRSF объекта
        shared.linkUp = crsf->isLinkUp();
        shared.lastReceive = crsf->_lastReceive;
        
        for (int i = 0; i < 16; i++) {
            shared.channels[i] = crsf->getChannel(i + 1);
        }
        
        // ... заполняем остальные поля
        
        // Записываем в файл
        std::ofstream file("/tmp/crsf_telemetry.dat", std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&shared), 
                      sizeof(SharedTelemetryData));
            file.close();
        }
        
        rpi_delay_ms(20);  // Обновление каждые 20мс для реалтайма
    }
});
telemetryWriterThread.detach();
```

**Важные детали:**
- Используется `std::ios::binary` для бинарной записи
- `reinterpret_cast<const char*>` для преобразования указателя на структуру в char*
- `sizeof(SharedTelemetryData)` гарантирует запись всей структуры
- Поток работает независимо от основного цикла
- Обновление каждые 20мс обеспечивает реалтайм (50 Гц)

### Чтение телеметрии в pybind

```cpp
TelemetryData getTelemetry() {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    TelemetryData data;
    
    std::ifstream file("/tmp/crsf_telemetry.dat", std::ios::binary);
    if (file.is_open() && file.good()) {
        SharedTelemetryData shared;
        file.read(reinterpret_cast<char*>(&shared), sizeof(SharedTelemetryData));
        file.close();
        
        // Проверяем, что прочитали правильное количество байт
        if (file.gcount() == sizeof(SharedTelemetryData)) {
            // Копируем данные
            data.linkUp = shared.linkUp;
            data.channels.assign(shared.channels, shared.channels + 16);
            // ...
        }
    }
    
    return data;
}
```

**Важные детали:**
- `std::lock_guard<std::mutex>` защищает от гонок данных
- Проверка `file.gcount()` гарантирует, что прочитали всю структуру
- `data.channels.assign()` копирует массив в vector для Python
- Если файл не найден или поврежден, возвращаются значения по умолчанию

### Обработка команд в main.cpp

```cpp
// Обработка команд из файла (от Python обертки)
std::ifstream cmdFile("/tmp/crsf_command.txt");
if (cmdFile.is_open()) {
    std::string cmd;
    // Обрабатываем ВСЕ команды из файла (многострочный формат)
    while (std::getline(cmdFile, cmd)) {
        if (cmd.find("setChannels") == 0) {
            // Формат: setChannels 1=1500 2=1600 3=1700 ...
            std::istringstream iss(cmd);
            std::string token;
            iss >> token; // пропускаем "setChannels"
            while (iss >> token) {
                size_t pos = token.find('=');
                if (pos != std::string::npos) {
                    unsigned int ch = std::stoi(token.substr(0, pos));
                    int value = std::stoi(token.substr(pos + 1));
                    if (ch >= 1 && ch <= 16 && value >= 1000 && value <= 2000) {
                        crsfSetChannel(ch, value);
                    }
                }
            }
        } else if (cmd.find("setChannel") == 0) {
            // Одиночная команда для обратной совместимости
            // ...
        }
    }
    cmdFile.close();
    // Удаляем файл только после обработки ВСЕХ команд
    remove("/tmp/crsf_command.txt");
}
```

**Важные детали:**
- `while (std::getline(...))` обрабатывает все строки файла
- Парсинг формата `канал=значение` через `std::istringstream`
- Валидация значений перед установкой
- Файл удаляется только после обработки всех команд

### Сборка pybind11 модуля

**Файл: `pybind/build_lib.py`**

```python
# Список исходных файлов для CRSF модуля
crsf_sources = [
    'src/crsf_bindings.cpp',
    os.path.join(project_root, 'libs/crsf/CrsfSerial.cpp'),
    os.path.join(project_root, 'libs/crsf/crc8.cpp'),
    os.path.join(project_root, 'libs/SerialPort.cpp'),
    os.path.join(project_root, 'libs/rpi_hal.cpp'),
]

# Директории с заголовками
include_dirs = [
    get_pybind_include(),
    project_root,
    os.path.join(project_root, 'libs'),
    os.path.join(project_root, 'libs/crsf'),
    os.path.join(project_root, 'crsf'),
]

# Флаги компиляции
compile_args = ['-std=c++17', '-O2', '-fPIC']
```

**Почему эти файлы:**
- `crsf_bindings.cpp` - основной pybind11 модуль
- `CrsfSerial.cpp` - класс для работы с CRSF
- `crc8.cpp` - вычисление CRC8 для CRSF пакетов
- `SerialPort.cpp` - работа с последовательным портом
- `rpi_hal.cpp` - HAL для Raspberry Pi (время, задержки)

**Почему не включаем `crsf/crsf.cpp`:**
- Он содержит статические объекты, которые инициализируются в main.cpp
- Включение его в pybind модуль создало бы дублирование объектов
- Основное приложение уже компилирует его отдельно

### Экспорт функций в pybind11

```cpp
PYBIND11_MODULE(crsf_native, m) {
    m.doc() = "CRSF Native C++ bindings for Python";
    
    // Экспорт структуры
    py::class_<TelemetryData>(m, "TelemetryData")
        .def_readwrite("linkUp", &TelemetryData::linkUp)
        // ...
    
    // Экспорт функций
    m.def("get_telemetry", &getTelemetry, "Get telemetry data");
    m.def("set_channel", &setChannel, "Set channel value",
          py::arg("channel"), py::arg("value"));
    m.def("set_channels", &setChannels, "Set all channels at once",
          py::arg("channels"));
    // ...
}
```

**Как это работает:**
- `PYBIND11_MODULE(crsf_native, m)` создает модуль с именем `crsf_native`
- `m.def()` регистрирует функцию для вызова из Python
- `py::arg()` задает имена параметров для Python
- pybind11 автоматически генерирует код конвертации типов

---

## Удаленные компоненты

### telemetry_server.cpp и telemetry_server.h

**Что было удалено:**
- Весь код HTTP сервера
- Функции `startTelemetryServer()`, `handleHttpRequest()`, `createTelemetryJson()`
- Обработка HTTP запросов
- Создание JSON ответов

**Почему удалено:**
- Больше не нужен HTTP сервер
- Данные передаются через файлы
- Упрощение архитектуры
- Меньше зависимостей

### Код из main.cpp

**Удалено:**
```cpp
// Запуск веб-сервера телеметрии в отдельном потоке
std::thread webServerThread([]() {
    rpi_delay_ms(500);
    void* crsfPtr = crsfGetActive();
    if (crsfPtr != nullptr) {
        startTelemetryServer((CrsfSerial*)crsfPtr, 8081, 10);
    }
});
webServerThread.detach();
```

**Заменено на:**
```cpp
// Поток для записи телеметрии в файл
std::thread telemetryWriterThread([&]() {
    // Запись в /tmp/crsf_telemetry.dat
});
telemetryWriterThread.detach();
```

### Код из crsf_realtime_interface.py

**Удалено:**
```python
import requests
# ...
response = requests.get(f"{self.api_url}/api/telemetry", timeout=2)
data = response.json()
```

**Заменено на:**
```python
from crsf_wrapper import CRSFWrapper
# ...
data = self.crsf.get_telemetry()
```

---

## Преимущества нового подхода

### 1. Производительность
- **Было:** HTTP запросы, JSON парсинг, сетевая задержка
- **Стало:** Прямое чтение бинарного файла, без парсинга, без сети
- **Результат:** Меньше задержка, выше частота обновления

### 2. Надежность
- **Было:** Зависимость от сетевого стека, возможны таймауты
- **Стало:** Файловая система, простая и надежная
- **Результат:** Меньше точек отказа

### 3. Простота
- **Было:** HTTP сервер, обработка запросов, JSON
- **Стало:** Запись/чтение файлов
- **Результат:** Проще код, проще отладка

### 4. Безопасность
- **Было:** HTTP сервер открыт на порту (потенциальная уязвимость)
- **Стало:** Файлы в /tmp (локальный доступ)
- **Результат:** Меньше поверхность атаки

---

## Итоговая структура файлов

```
CRSF-IO-mkII-main/
├── main.cpp                    # Основное приложение (запись телеметрии, обработка команд)
├── crsf_realtime_interface.py  # Python интерфейс (обновлен для использования обертки)
├── pybind/
│   ├── src/
│   │   └── crsf_bindings.cpp   # pybind11 модуль (чтение телеметрии, запись команд)
│   ├── crsf_wrapper.py         # Python обертка (удобный интерфейс)
│   ├── build_lib.py            # Скрипт сборки pybind11 модуля
│   └── README.md               # Документация
└── docs/
    └── PYBIND11_MIGRATION.md   # Этот файл
```

---

## Заключение

Миграция с HTTP API на pybind11 была выполнена для:
1. Улучшения производительности (прямой доступ к данным)
2. Упрощения архитектуры (без HTTP сервера)
3. Решения проблем с межпроцессным взаимодействием (файлы вместо указателей)
4. Обеспечения одновременной установки всех каналов (одна команда вместо множества)

Все проблемы были решены, система работает стабильно и эффективно.

