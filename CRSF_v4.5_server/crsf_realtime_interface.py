#!/usr/bin/env python3
"""
CRSF Realtime Interface
Интерфейс для отображения данных CRSF в реальном времени через Python обертку
Поддерживает работу как через pybind (на ведомом узле), так и через API (на ведущем узле)
"""

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time
from datetime import datetime
import queue
import sys
import os
import argparse

# Добавляем путь к pybind для импорта обертки (если она собрана)
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'pybind'))
try:
    from crsf_wrapper import CRSFWrapper
    WRAPPER_AVAILABLE = True
    WRAPPER_IMPORT_ERROR = None
except Exception as e:  # noqa: BLE001 - важно показать пользователю причину
    WRAPPER_AVAILABLE = False
    WRAPPER_IMPORT_ERROR = e

class CRSFRealtimeInterface:
    def __init__(self, root, use_api=False, api_url="http://localhost:8081"):
        """
        Инициализация интерфейса
        
        Args:
            root: Корневое окно tkinter
            use_api: Если True, использовать API обертку (для ведущего узла)
            api_url: URL API сервера (если use_api=True)
        """
        self.root = root
        self.root.title("CRSF Realtime Interface" + (" (API Mode)" if use_api else ""))
        self.root.geometry("1200x800")
        self.root.configure(bg='#2b2b2b')
        self.initialized = False
        
        # Настройки
        self.update_interval = 20  # Уменьшаем до 20мс для реалтайма
        self.is_running = False
        self.data_queue = queue.Queue()
        self.use_api = use_api
        self.crsf = None
        
        # CRSF обертка (универсальная - работает и через pybind, и через API)
        if not WRAPPER_AVAILABLE:
            messagebox.showerror(
                "Ошибка",
                f"CRSF обертка недоступна: {WRAPPER_IMPORT_ERROR}\n"
                "Соберите Python bindings (см. pybind/README.md) или установите зависимости для API режима.",
            )
            return
        
        try:
            if use_api:
                # API режим: явно указываем api_url через переменную окружения
                # так как CRSFWrapper.auto_init() использует os.environ.get('CRSF_API_URL')
                old_api_url = os.environ.get('CRSF_API_URL')
                os.environ['CRSF_API_URL'] = api_url
                try:
                    self.crsf = CRSFWrapper()
                    self.crsf.auto_init()
                    messagebox.showinfo("Информация", f"Подключено к API серверу: {api_url}\n\nТелеметрия будет получаться через API.")
                finally:
                    # Восстанавливаем старое значение
                    if old_api_url is not None:
                        os.environ['CRSF_API_URL'] = old_api_url
                    elif 'CRSF_API_URL' in os.environ:
                        del os.environ['CRSF_API_URL']
            else:
                # Автоматический режим: CRSFWrapper сам определит, использовать pybind или API
                self.crsf = CRSFWrapper()
                self.crsf.auto_init()
        except Exception as e:
            if use_api:
                messagebox.showerror(
                    "Ошибка",
                    f"Не удалось подключиться к API серверу: {e}\n\nУбедитесь, что:\n"
                    f"1. API сервер запущен\n2. URL правильный: {api_url}"
                )
            else:
                messagebox.showerror(
                    "Ошибка",
                    f"Не удалось инициализировать CRSF: {e}\n\nУбедитесь, что:\n"
                    "1. Основное приложение crsf_io_rpi запущено (для pybind режима)\n"
                    "2. Файл /tmp/crsf_telemetry.dat создаётся (для pybind режима)\n"
                    "3. Или API сервер запущен (для API режима)\n"
                    "4. Собрана pybind обёртка (crsf_native) для pybind режима"
                )
            return
        
        # Данные
        self.current_data = None
        self.connection_status = False
        self.initialized = True
        
        # Создаем интерфейс
        self.create_interface()
        
        # Запускаем обновление данных
        self.start_data_update()
    
    def create_interface(self):
        """Создание интерфейса"""
        # Главный фрейм
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Настройка растягивания
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        
        # Заголовок
        title_label = ttk.Label(main_frame, text="CRSF Realtime Interface", 
                               font=('Arial', 16, 'bold'))
        title_label.grid(row=0, column=0, columnspan=2, pady=(0, 20))
        
        # Панель управления
        self.create_control_panel(main_frame)
        
        # Панель статуса
        self.create_status_panel(main_frame)
        
        # Панель данных
        self.create_data_panel(main_frame)
        
        # Панель каналов
        self.create_channels_panel(main_frame)
        
        # Ноутбук с вкладками для телеметрии
        self.create_telemetry_notebook(main_frame)
    
    def create_control_panel(self, parent):
        """Панель управления"""
        control_frame = ttk.LabelFrame(parent, text="Управление", padding="10")
        control_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Интервал обновления
        ttk.Label(control_frame, text="Интервал (мс):").grid(row=0, column=0, sticky=tk.W)
        self.interval_var = tk.StringVar(value=str(self.update_interval))
        interval_entry = ttk.Entry(control_frame, textvariable=self.interval_var, width=10)
        interval_entry.grid(row=0, column=1, padx=(5, 10))
        
        # Кнопки
        self.start_button = ttk.Button(control_frame, text="Старт", command=self.start_monitoring)
        self.start_button.grid(row=0, column=2, padx=(5, 5))
        
        self.stop_button = ttk.Button(control_frame, text="Стоп", command=self.stop_monitoring, state='disabled')
        self.stop_button.grid(row=0, column=3, padx=(5, 5))
        
        # Режим работы
        ttk.Label(control_frame, text="Режим:").grid(row=0, column=4, sticky=tk.W, padx=(20, 5))
        self.mode_var = tk.StringVar(value="manual")  # По умолчанию ручной режим
        mode_combo = ttk.Combobox(control_frame, textvariable=self.mode_var, 
                                 values=["joystick", "manual"], width=15)
        mode_combo.grid(row=0, column=5, padx=(5, 10))
        
        mode_button = ttk.Button(control_frame, text="Установить режим", command=self.set_mode)
        mode_button.grid(row=0, column=6, padx=(5, 5))
    
    def create_status_panel(self, parent):
        """Панель статуса"""
        status_frame = ttk.LabelFrame(parent, text="Статус", padding="10")
        status_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Статус подключения
        if self.use_api:
            self.status_label = ttk.Label(status_frame, text="API режим", foreground="blue")
        else:
            self.status_label = ttk.Label(status_frame, text="Отключено", foreground="red")
        self.status_label.grid(row=0, column=0, sticky=tk.W)
        
        # Время последнего обновления
        self.last_update_label = ttk.Label(status_frame, text="Последнее обновление: Никогда")
        self.last_update_label.grid(row=0, column=1, sticky=tk.W, padx=(20, 0))
        
        # Активный порт/API URL (будет обновляться при получении данных)
        self.port_label = ttk.Label(status_frame, text="Порт: Неизвестно")
        self.port_label.grid(row=1, column=0, sticky=tk.W)
        
        # Статистика пакетов
        self.packets_label = ttk.Label(status_frame, text="Пакеты: 0/0/0")
        self.packets_label.grid(row=1, column=1, sticky=tk.W, padx=(20, 0))
    
    def create_data_panel(self, parent):
        """Панель основных данных"""
        data_frame = ttk.LabelFrame(parent, text="Основные данные", padding="10")
        data_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Время
        ttk.Label(data_frame, text="Время:").grid(row=0, column=0, sticky=tk.W)
        self.time_label = ttk.Label(data_frame, text="--:--:--", font=('Arial', 12, 'bold'))
        self.time_label.grid(row=0, column=1, sticky=tk.W, padx=(10, 0))
        
        # Режим работы
        ttk.Label(data_frame, text="Режим работы:").grid(row=0, column=2, sticky=tk.W, padx=(20, 0))
        self.work_mode_label = ttk.Label(data_frame, text="Неизвестно")
        self.work_mode_label.grid(row=0, column=3, sticky=tk.W, padx=(10, 0))
    
    def create_channels_panel(self, parent):
        """Панель каналов"""
        channels_frame = ttk.LabelFrame(parent, text="RC Каналы", padding="10")
        channels_frame.grid(row=4, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Создаем сетку для каналов
        self.channel_labels = []
        self.channel_bars = []
        
        for i in range(16):
            row = i // 4
            col = (i % 4) * 3
            
            # Номер канала
            ttk.Label(channels_frame, text=f"CH{i+1}:").grid(row=row, column=col, sticky=tk.W, padx=(0, 5))
            
            # Прогресс бар
            bar = ttk.Progressbar(channels_frame, length=100, mode='determinate')
            bar.grid(row=row, column=col+1, padx=(0, 5))
            self.channel_bars.append(bar)
            
            # Значение
            label = ttk.Label(channels_frame, text="1500", width=6)
            label.grid(row=row, column=col+2, sticky=tk.W)
            self.channel_labels.append(label)
    
    def create_telemetry_notebook(self, parent):
        """Панель телеметрии с ноутбуком"""
        telemetry_frame = ttk.LabelFrame(parent, text="Телеметрия", padding="10")
        telemetry_frame.grid(row=5, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Настройка растягивания
        parent.rowconfigure(5, weight=1)
        
        # Создаем notebook для вкладок
        notebook = ttk.Notebook(telemetry_frame)
        notebook.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Настройка растягивания
        telemetry_frame.columnconfigure(0, weight=1)
        telemetry_frame.rowconfigure(0, weight=1)
        
        # Вкладка GPS
        gps_frame = ttk.Frame(notebook)
        notebook.add(gps_frame, text="GPS")
        self.create_gps_tab(gps_frame)
        
        # Вкладка Батарея
        battery_frame = ttk.Frame(notebook)
        notebook.add(battery_frame, text="Батарея")
        self.create_battery_tab(battery_frame)
        
        # Вкладка Положение
        attitude_frame = ttk.Frame(notebook)
        notebook.add(attitude_frame, text="Положение")
        self.create_attitude_tab(attitude_frame)
        
        # Вкладка Сырые данные
        raw_frame = ttk.Frame(notebook)
        notebook.add(raw_frame, text="Сырые данные")
        self.create_raw_data_tab(raw_frame)
        
        # Вкладка Ручное управление
        manual_frame = ttk.Frame(notebook)
        notebook.add(manual_frame, text="Ручное управление")
        self.create_manual_control_tab(manual_frame)
    
    def create_gps_tab(self, parent):
        """Вкладка GPS"""
        # Широта
        ttk.Label(parent, text="Широта:").grid(row=0, column=0, sticky=tk.W, padx=10, pady=5)
        self.lat_label = ttk.Label(parent, text="0.000000", font=('Arial', 10, 'bold'))
        self.lat_label.grid(row=0, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Долгота
        ttk.Label(parent, text="Долгота:").grid(row=1, column=0, sticky=tk.W, padx=10, pady=5)
        self.lon_label = ttk.Label(parent, text="0.000000", font=('Arial', 10, 'bold'))
        self.lon_label.grid(row=1, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Высота
        ttk.Label(parent, text="Высота:").grid(row=2, column=0, sticky=tk.W, padx=10, pady=5)
        self.alt_label = ttk.Label(parent, text="0.0 м", font=('Arial', 10, 'bold'))
        self.alt_label.grid(row=2, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Скорость
        ttk.Label(parent, text="Скорость:").grid(row=3, column=0, sticky=tk.W, padx=10, pady=5)
        self.speed_label = ttk.Label(parent, text="0.0 км/ч", font=('Arial', 10, 'bold'))
        self.speed_label.grid(row=3, column=1, sticky=tk.W, padx=10, pady=5)
    
    def create_battery_tab(self, parent):
        """Вкладка Батарея"""
        # Напряжение
        ttk.Label(parent, text="Напряжение:").grid(row=0, column=0, sticky=tk.W, padx=10, pady=5)
        self.voltage_label = ttk.Label(parent, text="0.0 В", font=('Arial', 10, 'bold'))
        self.voltage_label.grid(row=0, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Ток
        ttk.Label(parent, text="Ток:").grid(row=1, column=0, sticky=tk.W, padx=10, pady=5)
        self.current_label = ttk.Label(parent, text="0 мА", font=('Arial', 10, 'bold'))
        self.current_label.grid(row=1, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Емкость
        ttk.Label(parent, text="Емкость:").grid(row=2, column=0, sticky=tk.W, padx=10, pady=5)
        self.capacity_label = ttk.Label(parent, text="0 мАч", font=('Arial', 10, 'bold'))
        self.capacity_label.grid(row=2, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Остаток
        ttk.Label(parent, text="Остаток:").grid(row=3, column=0, sticky=tk.W, padx=10, pady=5)
        self.remaining_label = ttk.Label(parent, text="0%", font=('Arial', 10, 'bold'))
        self.remaining_label.grid(row=3, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Прогресс бар для остатка
        self.battery_bar = ttk.Progressbar(parent, length=200, mode='determinate')
        self.battery_bar.grid(row=4, column=0, columnspan=2, padx=10, pady=10)
    
    def create_attitude_tab(self, parent):
        """Вкладка Положение"""
        # Roll
        ttk.Label(parent, text="Roll (Крен):").grid(row=0, column=0, sticky=tk.W, padx=10, pady=5)
        self.roll_label = ttk.Label(parent, text="0.0°", font=('Arial', 10, 'bold'))
        self.roll_label.grid(row=0, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Pitch
        ttk.Label(parent, text="Pitch (Тангаж):").grid(row=1, column=0, sticky=tk.W, padx=10, pady=5)
        self.pitch_label = ttk.Label(parent, text="0.0°", font=('Arial', 10, 'bold'))
        self.pitch_label.grid(row=1, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Yaw
        ttk.Label(parent, text="Yaw (Рысканье):").grid(row=2, column=0, sticky=tk.W, padx=10, pady=5)
        self.yaw_label = ttk.Label(parent, text="0.0°", font=('Arial', 10, 'bold'))
        self.yaw_label.grid(row=2, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Прогресс бары для углов
        ttk.Label(parent, text="Roll:").grid(row=3, column=0, sticky=tk.W, padx=10, pady=5)
        self.roll_bar = ttk.Progressbar(parent, length=200, mode='determinate')
        self.roll_bar.grid(row=3, column=1, padx=10, pady=5)
        
        ttk.Label(parent, text="Pitch:").grid(row=4, column=0, sticky=tk.W, padx=10, pady=5)
        self.pitch_bar = ttk.Progressbar(parent, length=200, mode='determinate')
        self.pitch_bar.grid(row=4, column=1, padx=10, pady=5)
        
        ttk.Label(parent, text="Yaw:").grid(row=5, column=0, sticky=tk.W, padx=10, pady=5)
        self.yaw_bar = ttk.Progressbar(parent, length=200, mode='determinate')
        self.yaw_bar.grid(row=5, column=1, padx=10, pady=5)
    
    def create_raw_data_tab(self, parent):
        """Вкладка Сырые данные Attitude"""
        # Заголовок
        title_label = ttk.Label(parent, text="Сырые значения ATTITUDE из CRSF", 
                               font=('Arial', 12, 'bold'))
        title_label.grid(row=0, column=0, columnspan=2, pady=(10, 20))
        
        # Roll Raw
        ttk.Label(parent, text="Roll Raw (int16_t):").grid(row=1, column=0, sticky=tk.W, padx=10, pady=5)
        self.roll_raw_label = ttk.Label(parent, text="0", font=('Arial', 10, 'bold'), 
                                       foreground='#00ff00')
        self.roll_raw_label.grid(row=1, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Pitch Raw
        ttk.Label(parent, text="Pitch Raw (int16_t):").grid(row=2, column=0, sticky=tk.W, padx=10, pady=5)
        self.pitch_raw_label = ttk.Label(parent, text="0", font=('Arial', 10, 'bold'), 
                                        foreground='#00ff00')
        self.pitch_raw_label.grid(row=2, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Yaw Raw
        ttk.Label(parent, text="Yaw Raw (int16_t):").grid(row=3, column=0, sticky=tk.W, padx=10, pady=5)
        self.yaw_raw_label = ttk.Label(parent, text="0", font=('Arial', 10, 'bold'), 
                                      foreground='#00ff00')
        self.yaw_raw_label.grid(row=3, column=1, sticky=tk.W, padx=10, pady=5)
        
        # Разделитель
        ttk.Separator(parent, orient='horizontal').grid(row=4, column=0, columnspan=2, 
                                                       sticky=(tk.W, tk.E), pady=20)
        
        # Информационный блок
        info_text = """Сырые значения ATTITUDE - это int16_t значения, полученные 
из CRSF пакета CRSF_FRAMETYPE_ATTITUDE (0x1E).

Формат: 6 байт (3 × int16_t, big-endian)
- bytes 0-1: Roll raw value
- bytes 2-3: Pitch raw value  
- bytes 4-5: Yaw raw value

Эти значения используются для расчета углов в градусах."""
        
        info_label = ttk.Label(parent, text=info_text, justify=tk.LEFT, 
                              font=('Arial', 9))
        info_label.grid(row=5, column=0, columnspan=2, padx=10, pady=10, sticky=tk.W)
    
    def create_manual_control_tab(self, parent):
        """Вкладка ручного управления каналами"""
        # Создаем scrollable frame для большого количества элементов
        canvas = tk.Canvas(parent, bg='#2b2b2b')
        scrollbar = ttk.Scrollbar(parent, orient="vertical", command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)
        
        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        canvas.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        scrollbar.grid(row=0, column=1, sticky=(tk.N, tk.S))
        
        # Настройка растягивания
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(0, weight=1)
        
        # Заголовок
        title_label = ttk.Label(scrollable_frame, text="Ручное управление каналами", 
                               font=('Arial', 12, 'bold'))
        title_label.grid(row=0, column=0, columnspan=4, pady=(10, 20))
        
        # Предупреждение о режиме
        warning_label = ttk.Label(scrollable_frame, 
                                 text="⚠ Для управления каналами переключитесь в ручной режим",
                                 foreground='orange', font=('Arial', 10, 'bold'))
        warning_label.grid(row=1, column=0, columnspan=4, pady=(0, 10))
        
        # Глобальные кнопки
        button_frame = ttk.Frame(scrollable_frame)
        button_frame.grid(row=2, column=0, columnspan=4, pady=(0, 20))
        
        all_center_button = ttk.Button(button_frame, text="Все в центр (1500)", 
                                       command=self.set_all_center)
        all_center_button.grid(row=0, column=0, padx=5)
        
        all_min_button = ttk.Button(button_frame, text="Все минимум (1000)", 
                                    command=self.set_all_min)
        all_min_button.grid(row=0, column=1, padx=5)
        
        all_max_button = ttk.Button(button_frame, text="Все максимум (2000)", 
                                    command=self.set_all_max)
        all_max_button.grid(row=0, column=2, padx=5)
        
        apply_all_button = ttk.Button(button_frame, text="Применить все", 
                                      command=self.apply_all_channels)
        apply_all_button.grid(row=0, column=3, padx=5)
        
        # Сохраняем элементы управления каналами
        self.manual_channel_vars = []
        self.manual_channel_scales = []
        self.manual_channel_entries = []
        
        # Заголовки колонок
        col_labels = ["Канал", "Значение", "Ползунок", "Действия"]
        for i, label in enumerate(col_labels):
            ttk.Label(scrollable_frame, text=label, font=('Arial', 10, 'bold')).grid(
                row=3, column=i, padx=5, pady=5)
        
        # Создаем управление для каждого канала
        for ch_num in range(16):
            row = ch_num + 4
            
            # Номер канала
            ch_name = self.get_channel_name(ch_num + 1)
            label = ttk.Label(scrollable_frame, text=f"CH{ch_num+1} ({ch_name})", width=20)
            label.grid(row=row, column=0, sticky=tk.W, padx=5, pady=2)
            
            # Поле ввода значения
            var = tk.StringVar(value="1500")
            entry = ttk.Entry(scrollable_frame, textvariable=var, width=8)
            entry.grid(row=row, column=1, padx=5, pady=2)
            self.manual_channel_vars.append(var)
            self.manual_channel_entries.append(entry)
            
            # Ползунок
            scale_var = tk.DoubleVar(value=1500.0)
            scale = ttk.Scale(scrollable_frame, from_=1000, to=2000, 
                             variable=scale_var, orient=tk.HORIZONTAL, length=300)
            scale.grid(row=row, column=2, padx=5, pady=2, sticky=(tk.W, tk.E))
            self.manual_channel_scales.append(scale)
            
            # Синхронизация ползунка и поля ввода
            def make_sync_handler(idx):
                def sync_to_entry(val):
                    if idx < len(self.manual_channel_vars):
                        self.manual_channel_vars[idx].set(f"{float(val):.0f}")
                return sync_to_entry
            
            def make_entry_handler(idx):
                def sync_to_scale(*args):
                    if idx < len(self.manual_channel_scales):
                        try:
                            val = int(self.manual_channel_vars[idx].get())
                            if 1000 <= val <= 2000:
                                self.manual_channel_scales[idx].set(val)
                        except ValueError:
                            pass
                return sync_to_scale
            
            scale.configure(command=make_sync_handler(ch_num))
            var.trace('w', make_entry_handler(ch_num))
            
            # Кнопки действий
            action_frame = ttk.Frame(scrollable_frame)
            action_frame.grid(row=row, column=3, padx=5, pady=2)
            
            center_button = ttk.Button(action_frame, text="Центр", width=8,
                                      command=lambda ch=ch_num+1, var=var, scale=scale: self.set_channel_center(ch, var, scale))
            center_button.grid(row=0, column=0, padx=2)
            
            apply_button = ttk.Button(action_frame, text="Применить", width=8,
                                     command=lambda ch=ch_num+1, var=var: self.apply_channel(ch, var))
            apply_button.grid(row=0, column=1, padx=2)
        
        # Обновляем размеры scrollable_frame
        scrollable_frame.update_idletasks()
        canvas.config(scrollregion=canvas.bbox("all"))
    
    def get_channel_name(self, ch_num):
        """Получить имя канала по номеру"""
        names = {
            1: "Roll", 2: "Pitch", 3: "Throttle", 4: "Yaw",
            5: "Aux1", 6: "Aux2", 7: "Aux3", 8: "Aux4",
            9: "Aux5", 10: "Aux6", 11: "Aux7", 12: "Aux8",
            13: "Aux9", 14: "Aux10", 15: "Aux11", 16: "Aux12"
        }
        return names.get(ch_num, f"Aux{ch_num-4}")
    
    def set_channel_center(self, ch_num, var, scale):
        """Установить канал в центр"""
        var.set("1500")
        scale.set(1500.0)
    
    def apply_channel(self, ch_num, var):
        """Применить значение канала"""
        try:
            value = int(var.get())
            if 1000 <= value <= 2000:
                self.send_channel_command(ch_num, value)
            else:
                messagebox.showerror("Ошибка", f"Значение должно быть от 1000 до 2000")
                var.set("1500")
        except ValueError:
            messagebox.showerror("Ошибка", "Неверное значение канала")
            var.set("1500")
    
    def set_all_center(self):
        """Установить все каналы в центр"""
        for var, scale in zip(self.manual_channel_vars, self.manual_channel_scales):
            var.set("1500")
            scale.set(1500.0)
    
    def set_all_min(self):
        """Установить все каналы в минимум"""
        for var, scale in zip(self.manual_channel_vars, self.manual_channel_scales):
            var.set("1000")
            scale.set(1000.0)
    
    def set_all_max(self):
        """Установить все каналы в максимум"""
        for var, scale in zip(self.manual_channel_vars, self.manual_channel_scales):
            var.set("2000")
            scale.set(2000.0)
    
    def apply_all_channels(self):
        """Применить все каналы одновременно"""
        try:
            if not self.crsf.is_initialized:
                messagebox.showerror("Ошибка", "CRSF не инициализирован")
                return
            
            # Собираем все значения каналов
            channels = []
            for ch_num in range(16):
                var = self.manual_channel_vars[ch_num]
                try:
                    value = int(var.get())
                    if 1000 <= value <= 2000:
                        channels.append(value)
                    else:
                        messagebox.showerror("Ошибка", f"Неверное значение для канала {ch_num + 1}: {value}")
                        return
                except ValueError:
                    messagebox.showerror("Ошибка", f"Неверное значение для канала {ch_num + 1}")
                    return
            
            # Устанавливаем все каналы одной командой
            self.crsf.set_channels(channels)
            
        except Exception as e:
            messagebox.showerror("Ошибка", f"Ошибка установки каналов: {e}")
    
    def send_channel_command(self, ch_num, value):
        """Отправить команду установки канала через обертку"""
        try:
            if not self.crsf.is_initialized:
                messagebox.showerror("Ошибка", "CRSF не инициализирован")
                return
            self.crsf.set_channel(ch_num, value)
        except Exception as e:
            messagebox.showerror("Ошибка", f"Ошибка установки канала {ch_num}: {e}")
    
    def start_monitoring(self):
        """Запуск мониторинга"""
        try:
            self.update_interval = int(self.interval_var.get())
            
            if not self.crsf.is_initialized:
                messagebox.showerror("Ошибка", "CRSF не инициализирован. Перезапустите приложение.")
                return
            
            self.is_running = True
            self.start_button.config(state='disabled')
            self.stop_button.config(state='normal')
            
            # Запускаем поток обновления данных
            self.update_thread = threading.Thread(target=self.data_update_worker, daemon=True)
            self.update_thread.start()
            
        except ValueError:
            messagebox.showerror("Ошибка", "Неверный интервал обновления")
    
    def stop_monitoring(self):
        """Остановка мониторинга"""
        self.is_running = False
        self.start_button.config(state='normal')
        self.stop_button.config(state='disabled')
        
        # Обновляем статус
        self.status_label.config(text="Остановлено", foreground="orange")
    
    def set_mode(self):
        """Установка режима работы"""
        if not self.crsf.is_initialized:
            messagebox.showerror("Ошибка", "CRSF не инициализирован")
            return
        
        mode = self.mode_var.get()
        try:
            self.crsf.set_work_mode(mode)
            messagebox.showinfo("Успех", f"Режим установлен: {mode}")
        except Exception as e:
            messagebox.showerror("Ошибка", f"Ошибка установки режима: {e}")
    
    def data_update_worker(self):
        """Поток обновления данных"""
        while self.is_running:
            try:
                if self.crsf.is_initialized:
                    # Получаем телеметрию (работает и через API, и через pybind)
                    data = self.crsf.get_telemetry()
                    self.data_queue.put(data)
                else:
                    self.data_queue.put(None)  # Ошибка
            except Exception as e:
                print(f"Ошибка получения данных: {e}")
                self.data_queue.put(None)
            
            time.sleep(self.update_interval / 1000.0)
    
    def start_data_update(self):
        """Запуск обновления интерфейса"""
        self.update_interface()
        self.root.after(20, self.start_data_update)  # Обновляем каждые 20мс для реалтайма
    
    def update_interface(self):
        """Обновление интерфейса"""
        # Обрабатываем очередь данных
        while not self.data_queue.empty():
            data = self.data_queue.get()
            if data:
                self.current_data = data
                self.connection_status = True
                self.update_display()
            else:
                self.connection_status = False
                self.status_label.config(text="Ошибка подключения", foreground="red")
    
    def update_display(self):
        """Обновление отображения данных"""
        if not self.current_data:
            return
        
        data = self.current_data
        
        # Статус подключения
        if self.use_api:
            # В API режиме проверяем linkUp, но показываем что это API
            if data.get('linkUp', False):
                self.status_label.config(text="API режим (Подключено)", foreground="green")
            else:
                self.status_label.config(text="API режим (Отключено)", foreground="orange")
        else:
            if data.get('linkUp', False):
                self.status_label.config(text="Подключено", foreground="green")
            else:
                self.status_label.config(text="Отключено", foreground="red")
        
        # Время последнего обновления
        current_time = datetime.now().strftime("%H:%M:%S")
        self.last_update_label.config(text=f"Последнее обновление: {current_time}")
        
        # Активный порт/API URL
        active_port = data.get('activePort', 'Неизвестно')
        if self.use_api:
            # Показываем API URL если доступен
            api_wrapper = getattr(self.crsf, '_api_wrapper', None)
            if api_wrapper and hasattr(api_wrapper, 'api_server_url'):
                self.port_label.config(text=f"API: {api_wrapper.api_server_url}")
            else:
                self.port_label.config(text=f"API: {active_port}")
        else:
            self.port_label.config(text=f"Порт: {active_port}")
        
        # Статистика пакетов
        packets_text = f"Пакеты: {data.get('packetsReceived', 0)}/{data.get('packetsSent', 0)}/{data.get('packetsLost', 0)}"
        self.packets_label.config(text=packets_text)
        
        # Время
        self.time_label.config(text=data.get('timestamp', '--:--:--'))
        
        # Режим работы
        self.work_mode_label.config(text=data.get('workMode', 'Неизвестно'))
        
        # Каналы
        channels = data.get('channels', [])
        for i, channel_value in enumerate(channels[:16]):
            if i < len(self.channel_labels):
                # Обновляем значение
                self.channel_labels[i].config(text=str(channel_value))
                
                # Обновляем прогресс бар (1000-2000 -> 0-100)
                progress = ((channel_value - 1000) / 1000) * 100
                self.channel_bars[i].config(value=progress)
        
        # GPS
        gps = data.get('gps', {})
        if gps:
            self.lat_label.config(text=f"{gps.get('latitude', 0):.6f}")
            self.lon_label.config(text=f"{gps.get('longitude', 0):.6f}")
            self.alt_label.config(text=f"{gps.get('altitude', 0):.1f} м")
            self.speed_label.config(text=f"{gps.get('speed', 0):.1f} км/ч")
        
        # Батарея
        battery = data.get('battery', {})
        if battery:
            voltage = battery.get('voltage', 0)
            current = battery.get('current', 0)
            capacity = battery.get('capacity', 0)
            remaining = battery.get('remaining', 0)
            
            self.voltage_label.config(text=f"{voltage:.1f} В")
            self.current_label.config(text=f"{current:.0f} мА")
            self.capacity_label.config(text=f"{capacity:.0f} мАч")
            self.remaining_label.config(text=f"{remaining}%")
            
            # Обновляем прогресс бар батареи
            self.battery_bar.config(value=remaining)
        
        # Положение
        attitude = data.get('attitude', {})
        if attitude:
            roll = attitude.get('roll', 0)
            pitch = attitude.get('pitch', 0)
            yaw = attitude.get('yaw', 0)
            
            self.roll_label.config(text=f"{roll:.1f}°")
            self.pitch_label.config(text=f"{pitch:.1f}°")
            self.yaw_label.config(text=f"{yaw:.1f}°")
            
            # Обновляем прогресс бары углов
            # Roll: -180 до 180 -> 0 до 100
            roll_progress = ((roll + 180) / 360) * 100
            self.roll_bar.config(value=roll_progress)
            
            # Pitch: -180 до 180 -> 0 до 100
            pitch_progress = ((pitch + 180) / 360) * 100
            self.pitch_bar.config(value=pitch_progress)
            
            # Yaw: 0 до 360 -> 0 до 100
            yaw_progress = (yaw / 360) * 100
            self.yaw_bar.config(value=yaw_progress)
        
        # Сырые значения attitude
        attitude_raw = data.get('attitudeRaw', {})
        if attitude_raw:
            roll_raw = attitude_raw.get('roll', 0)
            pitch_raw = attitude_raw.get('pitch', 0)
            yaw_raw = attitude_raw.get('yaw', 0)
            
            self.roll_raw_label.config(text=str(roll_raw))
            self.pitch_raw_label.config(text=str(pitch_raw))
            self.yaw_raw_label.config(text=str(yaw_raw))

def main():
    """Главная функция"""
    parser = argparse.ArgumentParser(description='CRSF Realtime Interface')
    parser.add_argument('--api', action='store_true', 
                       help='Использовать API режим (для работы на ведущем узле)')
    parser.add_argument('--api-url', type=str, default='http://localhost:8081',
                       help='URL API сервера (по умолчанию: http://localhost:8081)')
    
    args = parser.parse_args()
    
    root = tk.Tk()
    app = CRSFRealtimeInterface(root, use_api=args.api, api_url=args.api_url)
    if not app.initialized:
        root.destroy()
        return
    
    # Обработка закрытия окна
    def on_closing():
        app.stop_monitoring()
        root.destroy()
    
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()

if __name__ == "__main__":
    main()
