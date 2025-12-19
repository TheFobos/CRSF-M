#!/usr/bin/env python3
"""
CRSF Realtime Interface - Улучшенная версия
Интерфейс для управления CRSF через HTTP API с улучшенной обработкой ошибок,
управлением с клавиатуры и раздельными индикаторами TX/RX
"""

import tkinter as tk
from tkinter import ttk
import threading
import time
from datetime import datetime
import requests
import argparse
import sys


class CrsfClientApp:
    """Главный класс приложения для управления CRSF через API"""
    
    def __init__(self, root, host="localhost", port=8081):
        """
        Инициализация приложения
        
        Args:
            root: Корневое окно tkinter
            host: IP-адрес API сервера
            port: Порт API сервера
        """
        self.root = root
        self.root.title("CRSF Realtime Interface")
        self.root.geometry("1000x700")
        
        self.host = host
        self.port = port
        self.base_url = f"http://{host}:{port}"
        self.channels_url = f"{self.base_url}/api/command/setChannels"
        self.telemetry_url = f"{self.base_url}/api/telemetry"
        
        # Используем Session для переиспользования соединений
        self.session = requests.Session()
        self.session.headers.update({'Content-Type': 'application/json'})
        
        # Состояние приложения
        self.is_running = False
        self.channels = [1500] * 16  # 16 каналов, начальное значение 1500
        self.telemetry_data = {}
        
        # Потоки
        self.send_thread = None
        self.telemetry_thread = None
        
        # Создаем интерфейс
        self.create_interface()
        
        # Привязываем клавиатуру
        self.setup_keyboard_bindings()
        
        # Обработка закрытия окна
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
    
    def create_interface(self):
        """Создание интерфейса"""
        # Главный контейнер
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        
        # Заголовок
        title_label = ttk.Label(main_frame, text="CRSF Realtime Interface", 
                               font=('Arial', 16, 'bold'))
        title_label.grid(row=0, column=0, columnspan=2, pady=(0, 10))
        
        # Панель статуса
        self.create_status_panel(main_frame)
        
        # Панель управления каналами
        self.create_channels_panel(main_frame)
        
        # Панель телеметрии
        self.create_telemetry_panel(main_frame)
        
        # Панель управления
        self.create_control_panel(main_frame)
        
        # Строка статуса внизу
        self.create_status_bar(main_frame)
    
    def create_status_panel(self, parent):
        """Панель статуса с раздельными индикаторами TX/RX"""
        status_frame = ttk.LabelFrame(parent, text="Статус соединения", padding="10")
        status_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Индикатор TX (Uplink)
        ttk.Label(status_frame, text="TX (Uplink):", font=('Arial', 10, 'bold')).grid(
            row=0, column=0, sticky=tk.W, padx=(0, 10))
        self.tx_status_label = ttk.Label(status_frame, text="●", font=('Arial', 16))
        self.tx_status_label.grid(row=0, column=1, sticky=tk.W, padx=(0, 20))
        self.set_tx_status(False)
        
        # Индикатор RX (Downlink)
        ttk.Label(status_frame, text="RX (Downlink):", font=('Arial', 10, 'bold')).grid(
            row=0, column=2, sticky=tk.W, padx=(0, 10))
        self.rx_status_label = ttk.Label(status_frame, text="●", font=('Arial', 16))
        self.rx_status_label.grid(row=0, column=3, sticky=tk.W)
        self.set_rx_status(False)
        
        # URL подключения
        ttk.Label(status_frame, text=f"API: {self.base_url}").grid(
            row=1, column=0, columnspan=4, sticky=tk.W, pady=(10, 0))
    
    def create_channels_panel(self, parent):
        """Панель управления каналами со слайдерами"""
        channels_frame = ttk.LabelFrame(parent, text="RC Каналы", padding="10")
        channels_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Создаем слайдеры для каналов
        self.channel_scales = []
        self.channel_labels = []
        
        channel_names = ["Roll", "Pitch", "Throttle", "Yaw", 
                        "Aux1", "Aux2", "Aux3", "Aux4",
                        "Aux5", "Aux6", "Aux7", "Aux8",
                        "Aux9", "Aux10", "Aux11", "Aux12"]
        
        for i in range(16):
            row = i // 4
            col = (i % 4) * 3
            
            # Название канала
            name_label = ttk.Label(channels_frame, text=f"CH{i+1} ({channel_names[i]}):", width=15)
            name_label.grid(row=row, column=col, sticky=tk.W, padx=(0, 5))
            
            # Слайдер
            var = tk.IntVar(value=1500)
            scale = ttk.Scale(channels_frame, from_=1000, to=2000, 
                            variable=var, orient=tk.HORIZONTAL, length=150,
                            command=lambda val, idx=i: self.on_channel_change(idx, val))
            scale.grid(row=row, column=col+1, padx=(0, 5))
            self.channel_scales.append(scale)
            
            # Значение
            value_label = ttk.Label(channels_frame, text="1500", width=6)
            value_label.grid(row=row, column=col+2, sticky=tk.W)
            self.channel_labels.append(value_label)
    
    def create_telemetry_panel(self, parent):
        """Панель отображения телеметрии"""
        telemetry_frame = ttk.LabelFrame(parent, text="Телеметрия", padding="10")
        telemetry_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(0, 10))
        parent.rowconfigure(3, weight=1)
        
        # Создаем текстовую область для телеметрии
        self.telemetry_text = tk.Text(telemetry_frame, height=10, width=60, wrap=tk.WORD)
        scrollbar = ttk.Scrollbar(telemetry_frame, orient="vertical", command=self.telemetry_text.yview)
        self.telemetry_text.configure(yscrollcommand=scrollbar.set)
        
        self.telemetry_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        scrollbar.grid(row=0, column=1, sticky=(tk.N, tk.S))
        telemetry_frame.columnconfigure(0, weight=1)
        telemetry_frame.rowconfigure(0, weight=1)
        
        # Начальное сообщение
        self.telemetry_text.insert("1.0", "Ожидание телеметрии...\n")
        self.telemetry_text.config(state=tk.DISABLED)
    
    def create_control_panel(self, parent):
        """Панель управления"""
        control_frame = ttk.LabelFrame(parent, text="Управление", padding="10")
        control_frame.grid(row=4, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Кнопка Start/Stop
        self.start_stop_button = ttk.Button(control_frame, text="Старт", 
                                            command=self.toggle_start_stop)
        self.start_stop_button.grid(row=0, column=0, padx=5)
        
        # Кнопка Disarm (Стоп)
        disarm_button = ttk.Button(control_frame, text="Disarm (Space)", 
                                  command=self.disarm)
        disarm_button.grid(row=0, column=1, padx=5)
        
        # Кнопка Center All
        center_button = ttk.Button(control_frame, text="Center All", 
                                  command=self.center_all)
        center_button.grid(row=0, column=2, padx=5)
        
        # Информация об управлении
        info_label = ttk.Label(control_frame, 
                              text="Управление: W/S - Throttle, A/D - Yaw, Стрелки - Roll/Pitch",
                              font=('Arial', 9))
        info_label.grid(row=1, column=0, columnspan=3, pady=(10, 0))
    
    def create_status_bar(self, parent):
        """Строка статуса внизу окна"""
        self.status_bar = ttk.Label(parent, text="Готов к работе", 
                                   relief=tk.SUNKEN, anchor=tk.W)
        self.status_bar.grid(row=5, column=0, columnspan=2, sticky=(tk.W, tk.E))
    
    def setup_keyboard_bindings(self):
        """Настройка привязок клавиатуры"""
        self.root.bind('<KeyPress-w>', lambda e: self.adjust_channel(2, 10))  # Throttle вверх
        self.root.bind('<KeyPress-s>', lambda e: self.adjust_channel(2, -10))  # Throttle вниз
        self.root.bind('<KeyPress-a>', lambda e: self.adjust_channel(3, -10))  # Yaw влево
        self.root.bind('<KeyPress-d>', lambda e: self.adjust_channel(3, 10))  # Yaw вправо
        self.root.bind('<Up>', lambda e: self.adjust_channel(1, -10))  # Pitch вверх
        self.root.bind('<Down>', lambda e: self.adjust_channel(1, 10))  # Pitch вниз
        self.root.bind('<Left>', lambda e: self.adjust_channel(0, -10))  # Roll влево
        self.root.bind('<Right>', lambda e: self.adjust_channel(0, 10))  # Roll вправо
        self.root.bind('<space>', lambda e: self.disarm())  # Disarm
        
        # Фокус на окне для получения событий клавиатуры
        self.root.focus_set()
    
    def adjust_channel(self, channel_idx, delta):
        """Изменение значения канала на delta"""
        if not self.is_running:
            return
        
        current_value = self.channels[channel_idx]
        new_value = max(1000, min(2000, current_value + delta))
        self.channels[channel_idx] = new_value
        
        # Обновляем слайдер
        self.channel_scales[channel_idx].set(new_value)
        self.channel_labels[channel_idx].config(text=str(new_value))
    
    def on_channel_change(self, channel_idx, value):
        """Обработчик изменения слайдера"""
        int_value = int(float(value))
        self.channels[channel_idx] = int_value
        self.channel_labels[channel_idx].config(text=str(int_value))
    
    def set_tx_status(self, is_ok):
        """Установка статуса TX (зеленый/красный)"""
        if is_ok:
            self.tx_status_label.config(text="●", foreground="green")
        else:
            self.tx_status_label.config(text="●", foreground="red")
    
    def set_rx_status(self, is_ok):
        """Установка статуса RX (зеленый/красный)"""
        if is_ok:
            self.rx_status_label.config(text="●", foreground="green")
        else:
            self.rx_status_label.config(text="●", foreground="red")
    
    def update_status_bar(self, message, color="black"):
        """Обновление строки статуса"""
        self.status_bar.config(text=message, foreground=color)
    
    def send_channels_loop(self):
        """Цикл отправки каналов в отдельном потоке"""
        while self.is_running:
            try:
                # Отправляем каналы с тайм-аутом 100мс
                response = self.session.post(
                    self.channels_url,
                    json={"channels": self.channels},
                    timeout=0.1
                )
                
                if response.status_code == 200:
                    self.set_tx_status(True)
                    self.root.after(0, lambda: self.update_status_bar("TX: OK", "green"))
                else:
                    self.set_tx_status(False)
                    self.root.after(0, lambda: self.update_status_bar(
                        f"TX: Error {response.status_code}", "orange"))
                    
            except requests.exceptions.Timeout:
                self.set_tx_status(False)
                self.root.after(0, lambda: self.update_status_bar("TX: Timeout", "red"))
            except requests.exceptions.ConnectionError:
                self.set_tx_status(False)
                self.root.after(0, lambda: self.update_status_bar("TX: Disconnected", "red"))
            except Exception as e:
                # Логируем в консоль, но не блокируем интерфейс
                print(f"TX Error: {e}")
                self.set_tx_status(False)
                self.root.after(0, lambda: self.update_status_bar(f"TX: Error - {str(e)[:30]}", "red"))
            
            time.sleep(0.05)  # 20 раз в секунду
    
    def get_telemetry_loop(self):
        """Цикл получения телеметрии в отдельном потоке"""
        while self.is_running:
            try:
                # Получаем телеметрию с тайм-аутом 100мс
                response = self.session.get(self.telemetry_url, timeout=0.1)
                
                if response.status_code == 200:
                    data = response.json()
                    self.telemetry_data = data
                    
                    # Проверяем link_quality для RX статуса
                    link_quality = data.get('link_quality', 0)
                    has_data = link_quality > 0 or data.get('linkUp', False)
                    self.set_rx_status(has_data)
                    
                    # Обновляем отображение телеметрии
                    self.root.after(0, lambda d=data: self.update_telemetry_display(d))
                    self.root.after(0, lambda: self.update_status_bar("RX: OK", "green"))
                else:
                    self.set_rx_status(False)
                    self.root.after(0, lambda: self.update_status_bar(
                        f"RX: Error {response.status_code}", "orange"))
                    
            except requests.exceptions.Timeout:
                self.set_rx_status(False)
                self.root.after(0, lambda: self.update_status_bar("RX: Timeout", "red"))
            except requests.exceptions.ConnectionError:
                self.set_rx_status(False)
                self.root.after(0, lambda: self.update_status_bar("RX: Disconnected", "red"))
            except Exception as e:
                # Логируем в консоль, но не блокируем интерфейс
                print(f"RX Error: {e}")
                self.set_rx_status(False)
                self.root.after(0, lambda: self.update_status_bar(f"RX: Error - {str(e)[:30]}", "red"))
            
            time.sleep(0.1)  # 10 раз в секунду для телеметрии
    
    def update_telemetry_display(self, data):
        """Обновление отображения телеметрии"""
        self.telemetry_text.config(state=tk.NORMAL)
        self.telemetry_text.delete("1.0", tk.END)
        
        # Форматируем данные телеметрии
        timestamp = datetime.now().strftime("%H:%M:%S")
        text = f"Обновлено: {timestamp}\n\n"
        
        # Основная информация
        text += f"Link Up: {data.get('linkUp', False)}\n"
        text += f"Link Quality: {data.get('link_quality', 0)}\n"
        text += f"Active Port: {data.get('activePort', 'Unknown')}\n\n"
        
        # Каналы
        channels = data.get('channels', [])
        if channels:
            text += "Каналы:\n"
            for i, ch in enumerate(channels[:16]):
                text += f"  CH{i+1}: {ch}\n"
            text += "\n"
        
        # GPS
        gps = data.get('gps', {})
        if gps:
            text += f"GPS: Lat={gps.get('latitude', 0):.6f}, Lon={gps.get('longitude', 0):.6f}\n"
            text += f"  Alt={gps.get('altitude', 0):.1f}m, Speed={gps.get('speed', 0):.1f}km/h\n\n"
        
        # Батарея
        battery = data.get('battery', {})
        if battery:
            text += f"Батарея: {battery.get('voltage', 0):.1f}V, "
            text += f"{battery.get('current', 0):.0f}mA, "
            text += f"{battery.get('remaining', 0)}%\n\n"
        
        # Attitude
        attitude = data.get('attitude', {})
        if attitude:
            text += f"Attitude: Roll={attitude.get('roll', 0):.1f}°, "
            text += f"Pitch={attitude.get('pitch', 0):.1f}°, "
            text += f"Yaw={attitude.get('yaw', 0):.1f}°\n"
        
        self.telemetry_text.insert("1.0", text)
        self.telemetry_text.config(state=tk.DISABLED)
    
    def toggle_start_stop(self):
        """Переключение режима Start/Stop"""
        if not self.is_running:
            self.start()
        else:
            self.stop()
    
    def start(self):
        """Запуск отправки каналов и получения телеметрии"""
        self.is_running = True
        self.start_stop_button.config(text="Стоп")
        
        # Запускаем потоки
        self.send_thread = threading.Thread(target=self.send_channels_loop, daemon=True)
        self.send_thread.start()
        
        self.telemetry_thread = threading.Thread(target=self.get_telemetry_loop, daemon=True)
        self.telemetry_thread.start()
        
        self.update_status_bar("Запущено", "green")
    
    def stop(self):
        """Остановка отправки каналов и получения телеметрии"""
        self.is_running = False
        self.start_stop_button.config(text="Старт")
        
        # Ждем завершения потоков (максимум 1 секунда)
        if self.send_thread:
            self.send_thread.join(timeout=1.0)
        if self.telemetry_thread:
            self.telemetry_thread.join(timeout=1.0)
        
        self.set_tx_status(False)
        self.set_rx_status(False)
        self.update_status_bar("Остановлено", "orange")
    
    def disarm(self):
        """Disarm: устанавливает Throttle в 1000 и AUX1 в Disarm"""
        if not self.is_running:
            return
        
        # Throttle (CH3, индекс 2) в 1000
        self.channels[2] = 1000
        self.channel_scales[2].set(1000)
        self.channel_labels[2].config(text="1000")
        
        # AUX1 (CH5, индекс 4) в Disarm (обычно 1000)
        self.channels[4] = 1000
        self.channel_scales[4].set(1000)
        self.channel_labels[4].config(text="1000")
        
        self.update_status_bar("Disarm выполнено", "red")
    
    def center_all(self):
        """Установить все каналы в центр (1500)"""
        for i in range(16):
            self.channels[i] = 1500
            self.channel_scales[i].set(1500)
            self.channel_labels[i].config(text="1500")
        
        self.update_status_bar("Все каналы установлены в центр", "blue")
    
    def on_closing(self):
        """Обработка закрытия окна"""
        self.stop()
        # Закрываем сессию
        self.session.close()
        self.root.destroy()


def main():
    """Главная функция"""
    parser = argparse.ArgumentParser(
        description='CRSF Realtime Interface - Улучшенная версия',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры использования:
  python crsf_realtime_interface.py                    # Подключение к localhost:8081
  python crsf_realtime_interface.py --ip 192.168.1.100  # Подключение к удаленному серверу
  python crsf_realtime_interface.py --ip 192.168.1.100 --port 8081
        """
    )
    parser.add_argument('--ip', type=str, default='localhost',
                       help='IP-адрес API сервера (по умолчанию: localhost)')
    parser.add_argument('--port', type=int, default=8081,
                       help='Порт API сервера (по умолчанию: 8081)')
    
    args = parser.parse_args()
    
    root = tk.Tk()
    app = CrsfClientApp(root, host=args.ip, port=args.port)
    root.mainloop()


if __name__ == "__main__":
    main()
