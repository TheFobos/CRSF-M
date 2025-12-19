# Telemetry Documentation

Документация по телеметрии CRSF-IO-mkII

## Обзор

Система получает и обрабатывает телеметрию от полетного контроллера через CRSF протокол.

## Поддерживаемые типы телеметрии

| Тип | ID | Размер | Описание |
|-----|----|---------|----------|
| GPS | 0x02 | 15 bytes | GPS координаты, скорость, высота |
| BATTERY_SENSOR | 0x08 | 8 bytes | Напряжение, ток, емкость, остаток |
| ATTITUDE | 0x1E | 6 bytes | Roll, Pitch, Yaw углы |
| FLIGHT_MODE | 0x21 | Variable | Режим полета (строка) |
| LINK_STATISTICS | 0x14 | 10 bytes | Статистика связи |

## GPS

### Формат данных

```cpp
struct crsf_sensor_gps_t {
    uint32_t latitude;   // degree / 10,000,000
    uint32_t longitude;  // degree / 10,000,000
    uint16_t altitude;   // meters above sea level, +1000m offset
    uint16_t speed;      // km/h
    uint16_t heading;    // degree from north
};
```

### Конвертация

```json
{
  "latitude": 55.7558,      // GPS latitude / 10000000
  "longitude": 37.6173,     // GPS longitude / 10000000
  "altitude": 150.5,        // altitude - 1000
  "speed": 25.3             // speed / 10
}
```

## Battery Sensor

### Формат данных

```cpp
uint16_t voltage;    // mV
uint16_t current;    // mA
uint32_t capacity;   // mAh (24 bits)
uint8_t remaining;   // %
```

### Пример

```json
{
  "voltage": 12.6,      // Вольты
  "current": 1500,      // мА
  "capacity": 5000,     // мАч
  "remaining": 85       // %
}
```

## ATTITUDE

### Формат данных

CRSF ATTITUDE пакет содержит 6 байт (3 × int16_t, big-endian):

| Bytes | Поле | Тип | Описание |
|-------|------|-----|----------|
| 0-1 | Pitch | int16_t | Тангаж |
| 2-3 | Roll | int16_t | Крен |
| 4-5 | Yaw | int16_t | Рысканье |

### Конвертация

```cpp
int16_t pitch_raw = be16toh(*(int16_t*)&data[0]);
int16_t roll_raw = be16toh(*(int16_t*)&data[2]);
int16_t yaw_raw = be16toh(*(int16_t*)&data[4]);

double pitch = pitch_raw / 175.0;
double roll = roll_raw / 175.0;
double yaw = yaw_raw / 175.0;

// Yaw нормализуется к диапазону 0-360°
while (yaw < 0) yaw += 360.0;
while (yaw >= 360.0) yaw -= 360.0;
```

### Пример

```json
{
  "roll": -2.5,       // градусы (-180 до +180)
  "pitch": 1.2,       // градусы (-180 до +180)
  "yaw": 45.0         // градусы (0 до 360)
}
```

### Сырые значения

```json
{
  "attitudeRaw": {
    "roll": -437,     // int16_t из CRSF
    "pitch": 210,     // int16_t из CRSF
    "yaw": 7875       // int16_t из CRSF
  }
}
```

## Link Statistics

### Формат данных

```cpp
struct crsfLinkStatistics_t {
    uint8_t uplink_RSSI_1;
    uint8_t uplink_RSSI_2;
    uint8_t uplink_Link_quality;
    uint8_t uplink_SNR;
    int8_t active_antenna;
    uint8_t rf_Mode;
    uint8_t uplink_TX_Power;
    int8_t downlink_RSSI;
    uint8_t downlink_Link_quality;
    uint8_t downlink_SNR;
};
```

## Flight Mode

Строка с режимом полета, например:
- "ANGLE"
- "ACRO"
- "HORIZON"
- "AIR"

## Получение телеметрии

### HTTP GET

```bash
curl http://localhost:8081/api/telemetry
```

### Полный пример ответа

```json
{
  "linkUp": true,
  "activePort": "UART Active",
  "lastReceive": 1698345678,
  "timestamp": "12:34:56.789",
  "channels": [1500, 1500, 1000, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500],
  "packetsReceived": 1234,
  "packetsSent": 5678,
  "packetsLost": 0,
  "gps": {
    "latitude": 55.7558,
    "longitude": 37.6173,
    "altitude": 150.5,
    "speed": 25.3
  },
  "battery": {
    "voltage": 12.6,
    "current": 1500,
    "capacity": 5000,
    "remaining": 85
  },
  "attitude": {
    "roll": -2.5,
    "pitch": 1.2,
    "yaw": 45.0
  },
  "attitudeRaw": {
    "roll": -437,
    "pitch": 210,
    "yaw": 7875
  },
  "workMode": "joystick"
}
```

## Частота обновления

- **Телеметрия**: обновляется в реальном времени при получении пакетов от полетника
- **WebSocket**: 10 мс
- **HTTP API**: мгновенно при запросе

## Интерпретация данных

### GPS

- **Latitude/Longitude**: Отображаются в обычных градусах
- **Altitude**: Высота над уровнем моря в метрах (offset -1000 м убран)
- **Speed**: Скорость в км/ч

### Battery

- **Voltage**: Текущее напряжение батареи в Вольтах
- **Current**: Потребляемый ток в миллиамперах
- **Capacity**: Емкость в мАч
- **Remaining**: Процент оставшейся емкости

### Attitude

- **Roll**: Наклон вокруг продольной оси (-180° до +180°)
- **Pitch**: Наклон вокруг поперечной оси (-180° до +180°)
- **Yaw**: Поворот вокруг вертикальной оси (0° до 360°)

### Частота обновления CRSF

Разные типы пакетов отправляются с разной частотой:
- GPS: ~1-5 Hz
- Battery: ~1-10 Hz
- Attitude: ~10-50 Hz
- Link Statistics: ~1 Hz

## Python GUI

Включены графики и индикаторы для всех типов телеметрии:

- **GPS Tab**: Карта координат и высота
- **Battery Tab**: Напряжение и остаток
- **Attitude Tab**: Roll, Pitch, Yaw с визуализацией
- **Raw Data Tab**: Сырые значения из CRSF

Запуск:
```bash
python3 crsf_realtime_interface.py
```
