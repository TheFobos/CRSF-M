# Configuration Guide

Руководство по настройке CRSF-IO-mkII

## Файл конфигурации

Основной файл: `config.h`

## Основные настройки

### CRSF Protocol

```cpp
#define USE_CRSF_RECV true   // Включить прием CRSF от полетника
#define USE_CRSF_SEND true   // Включить отправку CRSF команд
#define USE_LOG false        // Логирование (влияет на производительность)
```

### Режимы работы устройства

```cpp
#define DEVICE_1 false  // Н-мост с ШИМ и направлением
#define DEVICE_2 false  // Сервоприводы 50 Гц (стандарт)
#define PIN_INIT false  // Инициализация дополнительных пинов (реле/камера)
```

### GPIO Пины (Raspberry Pi 5)

```cpp
#define motor_1_digital 17   // GPIO17 - направление мотора 1
#define motor_2_digital 27   // GPIO27 - направление мотора 2
#define motor_1_analog  18   // GPIO18 (PWM0) - ШИМ мотора 1
#define motor_2_analog  19   // GPIO19 (PWM1) - ШИМ мотора 2
#define rele_1 22            // GPIO22 - реле 1
#define rele_2 23            // GPIO23 - реле 2
#define camera 24            // GPIO24 - управление камерой
```

### PWM Configuration

```cpp
#define PWM_CHIP_M1 0  // pwmchip номер для мотора 1
#define PWM_NUM_M1  0  // номер канала внутри pwmchip для мотора 1
#define PWM_CHIP_M2 0  // pwmchip номер для мотора 2
#define PWM_NUM_M2  1  // номер канала внутри pwmchip для мотора 2
```

Для проверки доступных PWM:

```bash
ls -la /sys/class/pwm/
```

### UART Ports

```cpp
#define CRSF_PORT_PRIMARY "/dev/ttyAMA0"   // Основной порт (PL011)
#define CRSF_PORT_SECONDARY "/dev/ttyS0"   // Дополнительный порт (miniUART)
```

Проверка доступных портов:

```bash
ls -la /dev/tty*
```

Включение UART на Raspberry Pi:

```bash
sudo raspi-config
# Interfacing Options -> Serial -> Enable
```

### Baud Rate

```cpp
#define SERIAL_BAUD 115200   // Обычная скорость для отладки
#define CRSF_BAUD 420000     // Скорость CRSF протокола
```

## Настройки CRSF

### Timeout и Fail-safe

В файле `libs/crsf/CrsfSerial.h`:

```cpp
static const unsigned int CRSF_PACKET_TIMEOUT_MS = 100;      // Таймаут пакета
static const unsigned int CRSF_FAILSAFE_STAGE1_MS = 120000;  // Fail-safe (2 минуты)
```

### Attitude конвертация

В файле `libs/crsf/CrsfSerial.cpp`:

```cpp
// Коэффициент конвертации из raw в градусы
_attitudeRoll = rawVal2 / 175.0;
_attitudePitch = rawVal0 / 175.0;
_attitudeYaw = rawVal4 / 175.0;
```

## Примеры конфигурации

### Для сервоприводов (DEVICE_2)

```cpp
#define DEVICE_2 true
#define USE_CRSF_RECV true
#define USE_CRSF_SEND true
#define USE_LOG false
```

### Для Н-моста (DEVICE_1)

```cpp
#define DEVICE_1 true
#define USE_CRSF_RECV true
#define USE_CRSF_SEND true
#define USE_LOG false
```

### Для отладки

```cpp
#define USE_LOG true  // Включить логирование
#define USE_CRSF_RECV true
#define USE_CRSF_SEND true
```

### Минимальная конфигурация (только прием)

```cpp
#define USE_CRSF_RECV true
#define USE_CRSF_SEND false
#define DEVICE_1 false
#define DEVICE_2 false
#define PIN_INIT false
```

## После изменения конфигурации

```bash
make clean
make
sudo ./crsf_io_rpi
```

## Проверка конфигурации

Проверка GPIO:

```bash
gpio readall  # Требует wiringPi
```

Проверка PWM:

```bash
cat /sys/class/pwm/pwmchip0/pwm0/period
cat /sys/class/pwm/pwmchip0/pwm0/duty_cycle
```

Проверка UART:

```bash
dmesg | grep tty
```

## Устранение проблем

### UART не работает

1. Проверьте включен ли UART: `sudo raspi-config`
2. Проверьте права доступа: `sudo chmod 666 /dev/ttyAMA0`
3. Добавьте пользователя в группу dialout: `sudo usermod -a -G dialout $USER`

### PWM не работает

1. Проверьте доступность PWM: `ls /sys/class/pwm/`
2. Убедитесь что не используется другим процессом
3. Проверьте настройки в `config.h`

### Нет данных от полетника

1. Проверьте скорость: `CRSF_BAUD = 420000`
2. Проверьте порт: правильный ли `/dev/ttyAMA0` или `/dev/ttyS0`
3. Проверьте подключение: RX к TX, TX к RX
4. Включите логирование: `USE_LOG true`

## Оптимизация производительности

### Для максимальной производительности:

```cpp
#define USE_LOG false  // Отключить логирование
```

### Для отладки:

```cpp
#define USE_LOG true   // Включить логирование
```

## Дополнительные настройки

### Адрес веб-сервера

По умолчанию: `http://localhost:8081`

Изменение в `telemetry_server.cpp`:

```cpp
startTelemetryServer(crsfInstance, 8081, 10);  // Порт, частота обновления
```

### Частота отправки RC-каналов

По умолчанию: 100 Гц (каждые 10 мс)

Изменение в `main.cpp`:

```cpp
const uint32_t crsfSendPeriodMs = 10;  // миллисекунды
```
