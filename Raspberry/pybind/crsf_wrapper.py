#!/usr/bin/env python3
"""
CRSF Python Wrapper
Универсальная обертка для работы с CRSF через pybind11 или API
Автоматически определяет, что использовать: crsf_io_rpi (pybind) или api_server (API)
"""

import ctypes
import os
import sys

# Пытаемся импортировать pybind модуль
try:
    import crsf_native
    PYBIND_AVAILABLE = True
except ImportError:
    PYBIND_AVAILABLE = False
    crsf_native = None

# Пытаемся импортировать API обертку
try:
    # Добавляем родительскую директорию для импорта api_wrapper
    parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if parent_dir not in sys.path:
        sys.path.insert(0, parent_dir)
    from api_wrapper import CRSFAPIWrapper
    API_AVAILABLE = True
except ImportError:
    API_AVAILABLE = False
    CRSFAPIWrapper = None

from typing import Dict, List, Optional


class CRSFWrapper:
    """
    Универсальная Python обертка для CRSF
    
    Автоматически определяет, что использовать:
    - Если запущен crsf_io_rpi (файл /tmp/crsf_telemetry.dat существует) -> использует pybind
    - Если запущен api_server -> использует API
    - Программа не знает, через что работает - единый интерфейс
    """
    
    def __init__(self, crsf_ptr=None, api_url=None):
        """
        Инициализация обертки
        
        Args:
            crsf_ptr: Указатель на C++ CRSF объект (только для pybind режима)
            api_url: URL API сервера (если нужно явно указать API режим)
        """
        self._initialized = False
        self._backend = None  # 'pybind' или 'api'
        self._pybind_wrapper = None
        self._api_wrapper = None
        
        if crsf_ptr is not None:
            # Явная инициализация через pybind
            self.init(crsf_ptr)
        elif api_url is not None:
            # Явная инициализация через API
            if not API_AVAILABLE:
                raise RuntimeError("API обертка недоступна. Убедитесь, что api_wrapper.py существует.")
            self._api_wrapper = CRSFAPIWrapper(api_url)
            self._backend = 'api'
    
    def init(self, crsf_ptr):
        """
        Инициализация CRSF экземпляра через pybind (только для pybind режима)
        
        Args:
            crsf_ptr: Указатель на CRSF объект (int, ctypes.c_void_p или uintptr_t)
        """
        if not PYBIND_AVAILABLE:
            raise RuntimeError("Pybind модуль недоступен. Соберите crsf_native.")
        
        # Преобразуем указатель в целое число
        if isinstance(crsf_ptr, ctypes.c_void_p):
            ptr_value = crsf_ptr.value
        elif isinstance(crsf_ptr, int):
            ptr_value = crsf_ptr
        else:
            ptr_value = int(crsf_ptr)
        
        crsf_native.init_crsf_instance(ptr_value)
        self._backend = 'pybind'
        self._initialized = True
    
    def auto_init(self):
        """
        Автоматическая инициализация CRSF
        
        Автоматически определяет, что использовать:
        1. Проверяет наличие файла /tmp/crsf_telemetry.dat (crsf_io_rpi запущен) -> pybind
        2. Если файла нет, пробует подключиться к API серверу -> API
        3. Программа не знает, через что работает - единый интерфейс
        """
        # Шаг 1: Проверяем, запущен ли crsf_io_rpi (pybind режим)
        telemetry_file = "/tmp/crsf_telemetry.dat"
        if os.path.exists(telemetry_file) and PYBIND_AVAILABLE:
            try:
                # Файл существует, значит crsf_io_rpi запущен - используем pybind
                # Проверяем, что модуль действительно работает
                test_data = crsf_native.get_telemetry()
                self._backend = 'pybind'
                self._initialized = True
                return
            except Exception as e:
                # Если что-то пошло не так с pybind, пробуем API
                pass
        
        # Шаг 2: Пробуем подключиться к API серверу
        if API_AVAILABLE:
            try:
                api_url = os.environ.get('CRSF_API_URL', 'http://localhost:8081')
                self._api_wrapper = CRSFAPIWrapper(api_url)
                self._api_wrapper.auto_init()
                self._backend = 'api'
                self._initialized = True
                return
            except Exception as e:
                # API сервер недоступен
                pass
        
        # Если ничего не сработало
        raise RuntimeError(
            "Не удалось подключиться к CRSF. Убедитесь, что:\n"
            "  1. crsf_io_rpi запущен (для pybind режима)\n"
            "  2. Или api_server запущен (для API режима)"
        )
    
    def get_telemetry(self) -> Dict:
        """
        Получить телеметрию
        
        Returns:
            Словарь с данными телеметрии в формате, совместимом с API
        """
        if not self._initialized:
            raise RuntimeError("CRSF не инициализирован. Вызовите auto_init() сначала.")
        
        if self._backend == 'api':
            return self._api_wrapper.get_telemetry()
        elif self._backend == 'pybind':
            # Данные читаются из файла безопасным способом (без прямого доступа к указателю)
            data = crsf_native.get_telemetry()
            
            # Преобразуем в формат, совместимый с API
            return {
                'linkUp': data.linkUp,
                'activePort': data.activePort,
                'lastReceive': data.lastReceive,
                'timestamp': data.timestamp,
                'channels': data.channels,
                'packetsReceived': data.packetsReceived,
                'packetsSent': data.packetsSent,
                'packetsLost': data.packetsLost,
                'gps': {
                    'latitude': data.latitude,
                    'longitude': data.longitude,
                    'altitude': data.altitude,
                    'speed': data.speed
                },
                'battery': {
                    'voltage': data.voltage,
                    'current': data.current,
                    'capacity': data.capacity,
                    'remaining': data.remaining
                },
                'attitude': {
                    'roll': data.roll,
                    'pitch': data.pitch,
                    'yaw': data.yaw
                },
                'attitudeRaw': {
                    'roll': data.rollRaw,
                    'pitch': data.pitchRaw,
                    'yaw': data.yawRaw
                },
                'workMode': self.get_work_mode()
            }
        else:
            raise RuntimeError("Неизвестный backend")
    
    def set_work_mode(self, mode: str):
        """
        Установить режим работы
        
        Args:
            mode: 'joystick' или 'manual'
        """
        if not self._initialized:
            raise RuntimeError("CRSF не инициализирован. Вызовите auto_init() сначала.")
        
        if mode not in ['joystick', 'manual']:
            raise ValueError(f"Неверный режим: {mode}. Допустимые значения: 'joystick', 'manual'")
        
        if self._backend == 'api':
            self._api_wrapper.set_work_mode(mode)
        elif self._backend == 'pybind':
            crsf_native.set_work_mode(mode)
        else:
            raise RuntimeError("Неизвестный backend")
    
    def get_work_mode(self) -> str:
        """
        Получить текущий режим работы
        
        Returns:
            'joystick' или 'manual'
        """
        if not self._initialized:
            raise RuntimeError("CRSF не инициализирован. Вызовите auto_init() сначала.")
        
        if self._backend == 'api':
            return self._api_wrapper.get_work_mode()
        elif self._backend == 'pybind':
            return crsf_native.get_work_mode()
        else:
            raise RuntimeError("Неизвестный backend")
    
    def set_channel(self, channel: int, value: int):
        """
        Установить значение канала
        
        Args:
            channel: Номер канала (1-16)
            value: Значение (1000-2000)
        """
        if not self._initialized:
            raise RuntimeError("CRSF не инициализирован. Вызовите auto_init() сначала.")
        
        if not (1 <= channel <= 16):
            raise ValueError(f"Номер канала должен быть от 1 до 16, получено: {channel}")
        if not (1000 <= value <= 2000):
            raise ValueError(f"Значение канала должно быть от 1000 до 2000, получено: {value}")
        
        if self._backend == 'api':
            self._api_wrapper.set_channel(channel, value)
        elif self._backend == 'pybind':
            crsf_native.set_channel(channel, value)
        else:
            raise RuntimeError("Неизвестный backend")
    
    def set_channels(self, channels: List[int]):
        """
        Установить все каналы одновременно
        
        Args:
            channels: Список значений каналов (16 элементов, 1000-2000)
        """
        if not self._initialized:
            raise RuntimeError("CRSF не инициализирован. Вызовите auto_init() сначала.")
        
        if len(channels) < 16:
            raise ValueError(f"Должно быть 16 каналов, получено: {len(channels)}")
        
        if self._backend == 'api':
            self._api_wrapper.set_channels(channels[:16])
        elif self._backend == 'pybind':
            crsf_native.set_channels(channels[:16])
        else:
            raise RuntimeError("Неизвестный backend")
    
    def send_channels(self):
        """Отправить пакет каналов"""
        if not self._initialized:
            raise RuntimeError("CRSF не инициализирован. Вызовите auto_init() сначала.")
        
        if self._backend == 'api':
            self._api_wrapper.send_channels()
        elif self._backend == 'pybind':
            crsf_native.send_channels()
        else:
            raise RuntimeError("Неизвестный backend")
    
    @property
    def is_initialized(self) -> bool:
        """Проверка инициализации"""
        return self._initialized

