# Build Guide

Руководство по сборке проекта CRSF-IO-mkII

## Требования

- C++17 компилятор (g++ или clang++)
- Make
- Raspberry Pi или совместимая Linux система
- Права sudo для работы с GPIO/UART

## Быстрая сборка

```bash
make clean
make
```

## Доступные команды

### make (или make all)

Собрать основной исполняемый файл `crsf_io_rpi`.

```bash
make
```

### make clean

Удалить все скомпилированные файлы (*.o) и исполняемые файлы.

```bash
make clean
```

### make uart_test

Собрать утилиту для тестирования UART.

```bash
make uart_test
```

## Результаты сборки

После успешной сборки будут созданы:

- `crsf_io_rpi` - основной исполняемый файл
- `uart_test` - утилита для тестирования UART (опционально)
- `*.o` - объектные файлы

## Запуск

```bash
sudo ./crsf_io_rpi
```

Требуются права sudo для работы с GPIO и UART.

## Параметры сборки

Основные параметры в Makefile:

```makefile
CXX = g++                           # Компилятор C++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic  # Флаги компиляции
LDFLAGS = -lpthread                 # Библиотеки
```

### Оптимизация

- `-O2` - оптимизация скорости выполнения
- `-std=c++17` - стандарт C++17

### Предупреждения

- `-Wall` - включить все стандартные предупреждения
- `-Wextra` - дополнительные предупреждения
- `-Wpedantic` - строгое соответствие стандарту

## Структура проекта

```
CRSF-IO-mkII/
├── main.cpp
├── Makefile
├── config.h
├── crsf/
│   ├── crsf.cpp
│   └── crsf.h
├── libs/
│   ├── crsf/
│   │   ├── CrsfSerial.cpp
│   │   ├── CrsfSerial.h
│   │   ├── crsf_protocol.h
│   │   └── crc8.cpp
│   ├── SerialPort.cpp
│   └── rpi_hal.cpp
└── telemetry_server.cpp
```

## Устранение проблем сборки

### Ошибка компилятора

Если компилятор не найден:

```bash
sudo apt update
sudo apt install g++ make
```

### Ошибка зависимостей

Проверьте наличие всех заголовочных файлов:

```bash
find . -name "*.h" -o -name "*.hpp"
```

### Ошибка линковки

Проверьте наличие библиотеки pthread:

```bash
ls /usr/lib/libpthread*
```

## Кросс-компиляция

Для кросс-компиляции на другой машине:

1. Установите toolchain для Raspberry Pi
2. Измените `CXX` в Makefile
3. Соберите проект

```makefile
CXX = arm-linux-gnueabihf-g++
```

## Отладка

Для отладочной сборки измените флаги оптимизации:

```makefile
CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -Wpedantic
```

Флаги:
- `-g` - включить отладочную информацию
- `-O0` - отключить оптимизацию

## Очистка

Для полной очистки:

```bash
make clean
rm -f *.o crsf_io_rpi uart_test
```

## Проверка сборки

После сборки проверьте:

```bash
file crsf_io_rpi
ldd crsf_io_rpi
```
