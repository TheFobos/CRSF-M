#!/usr/bin/env python3
"""
API Wrapper для CRSF
Обертка для работы с CRSF через HTTP API (для использования на ведущем узле)
"""

import requests
from typing import Dict, List, Optional
import json


class CRSFAPIWrapper:
    """Python обертка для CRSF через HTTP API"""
    
    def __init__(self, api_server_url: str = "http://localhost:8081"):
        """
        Инициализация обертки
        
        Args:
            api_server_url: URL API сервера (например, "http://localhost:8081")
        """
        self.api_server_url = api_server_url.rstrip('/')
        self._initialized = True  # API всегда инициализирован
    
    def auto_init(self):
        """
        Автоматическая инициализация (для совместимости с CRSFWrapper)
        Проверяет доступность API сервера
        """
        try:
            response = requests.get(f"{self.api_server_url}/", timeout=2)
            if response.status_code == 200:
                self._initialized = True
                return
        except Exception as e:
            raise RuntimeError(f"Не удалось подключиться к API серверу {self.api_server_url}: {e}")
    
    def get_telemetry(self) -> Dict:
        """
        Получить телеметрию через API сервер
        
        Returns:
            Словарь с данными телеметрии
        """
        url = f"{self.api_server_url}/api/telemetry"
        
        try:
            response = requests.get(url, timeout=2)
            response.raise_for_status()
            data = response.json()
            
            # Преобразуем в формат, совместимый с CRSFWrapper
            return {
                'linkUp': data.get('linkUp', False),
                'activePort': data.get('activePort', 'API Server'),
                'lastReceive': data.get('lastReceive', 0),
                'timestamp': data.get('timestamp', ''),
                'channels': data.get('channels', [1500] * 16),
                'packetsReceived': data.get('packetsReceived', 0),
                'packetsSent': data.get('packetsSent', 0),
                'packetsLost': data.get('packetsLost', 0),
                'gps': data.get('gps', {
                    'latitude': 0.0,
                    'longitude': 0.0,
                    'altitude': 0.0,
                    'speed': 0.0
                }),
                'battery': data.get('battery', {
                    'voltage': 0.0,
                    'current': 0.0,
                    'capacity': 0.0,
                    'remaining': 0
                }),
                'attitude': data.get('attitude', {
                    'roll': 0.0,
                    'pitch': 0.0,
                    'yaw': 0.0
                }),
                'attitudeRaw': data.get('attitudeRaw', {
                    'roll': 0,
                    'pitch': 0,
                    'yaw': 0
                }),
                'workMode': self.get_work_mode()
            }
        except requests.exceptions.RequestException as e:
            # В случае ошибки возвращаем пустые данные
            return {
                'linkUp': False,
                'activePort': 'API Server (Error)',
                'lastReceive': 0,
                'timestamp': '',
                'channels': [1500] * 16,
                'packetsReceived': 0,
                'packetsSent': 0,
                'packetsLost': 0,
                'gps': {
                    'latitude': 0.0,
                    'longitude': 0.0,
                    'altitude': 0.0,
                    'speed': 0.0
                },
                'battery': {
                    'voltage': 0.0,
                    'current': 0.0,
                    'capacity': 0.0,
                    'remaining': 0
                },
                'attitude': {
                    'roll': 0.0,
                    'pitch': 0.0,
                    'yaw': 0.0
                },
                'attitudeRaw': {
                    'roll': 0,
                    'pitch': 0,
                    'yaw': 0
                },
                'workMode': 'manual'
            }
    
    def set_work_mode(self, mode: str):
        """
        Установить режим работы
        
        Args:
            mode: 'joystick' или 'manual'
        """
        if mode not in ['joystick', 'manual']:
            raise ValueError(f"Неверный режим: {mode}. Допустимые значения: 'joystick', 'manual'")
        
        url = f"{self.api_server_url}/api/command/setMode"
        data = {"mode": mode}
        
        try:
            response = requests.post(url, json=data, timeout=5)
            response.raise_for_status()
            result = response.json()
            if result.get('status') != 'ok':
                raise RuntimeError(f"Ошибка установки режима: {result.get('message', 'Unknown error')}")
        except requests.exceptions.RequestException as e:
            raise RuntimeError(f"Ошибка отправки команды: {e}")
    
    def get_work_mode(self) -> str:
        """
        Получить текущий режим работы
        
        ВАЖНО: API сервер не предоставляет текущий режим.
        Возвращает значение по умолчанию.
        
        Returns:
            'manual' (по умолчанию)
        """
        return "manual"  # API сервер не предоставляет текущий режим
    
    def set_channel(self, channel: int, value: int):
        """
        Установить значение канала
        
        Args:
            channel: Номер канала (1-16)
            value: Значение (1000-2000)
        """
        if not (1 <= channel <= 16):
            raise ValueError(f"Номер канала должен быть от 1 до 16, получено: {channel}")
        if not (1000 <= value <= 2000):
            raise ValueError(f"Значение канала должно быть от 1000 до 2000, получено: {value}")
        
        url = f"{self.api_server_url}/api/command/setChannel"
        data = {"channel": channel, "value": value}
        
        try:
            response = requests.post(url, json=data, timeout=5)
            response.raise_for_status()
            result = response.json()
            if result.get('status') != 'ok':
                raise RuntimeError(f"Ошибка установки канала: {result.get('message', 'Unknown error')}")
        except requests.exceptions.RequestException as e:
            raise RuntimeError(f"Ошибка отправки команды: {e}")
    
    def set_channels(self, channels: List[int]):
        """
        Установить все каналы одновременно
        
        Args:
            channels: Список значений каналов (16 элементов, 1000-2000)
        """
        if len(channels) < 16:
            raise ValueError(f"Должно быть 16 каналов, получено: {len(channels)}")
        
        url = f"{self.api_server_url}/api/command/setChannels"
        data = {"channels": channels[:16]}
        
        try:
            response = requests.post(url, json=data, timeout=5)
            response.raise_for_status()
            result = response.json()
            if result.get('status') != 'ok':
                raise RuntimeError(f"Ошибка установки каналов: {result.get('message', 'Unknown error')}")
        except requests.exceptions.RequestException as e:
            raise RuntimeError(f"Ошибка отправки команды: {e}")
    
    def send_channels(self):
        """Отправить пакет каналов"""
        url = f"{self.api_server_url}/api/command/sendChannels"
        data = {}
        
        try:
            response = requests.post(url, json=data, timeout=5)
            response.raise_for_status()
            result = response.json()
            if result.get('status') != 'ok':
                raise RuntimeError(f"Ошибка отправки каналов: {result.get('message', 'Unknown error')}")
        except requests.exceptions.RequestException as e:
            raise RuntimeError(f"Ошибка отправки команды: {e}")
    
    @property
    def is_initialized(self) -> bool:
        """Проверка инициализации"""
        return self._initialized
