import tkinter as tk
from tkinter import ttk
import requests
import threading
import time
import json
import argparse

class TelemetryApp:
    def __init__(self, root, api_url, interval):
        self.root = root
        self.root.title("CRSF IO Interface (Mark II)")
        self.api_url = api_url
        self.interval = interval
        self.running = True

        # Данные каналов (16 каналов, центр 1500)
        self.channels = [1500] * 16
        self.channels[0] = 1000 # Throttle в 0 по умолчанию
        
        # Для ограничения частоты отправки (Anti-Flood)
        self.last_send_time = 0
        self.send_interval = 0.05  # Не чаще чем раз в 50мс

        # GUI переменные
        self.telemetry_vars = {
            "Voltage": tk.StringVar(value="0.0 V"),
            "Current": tk.StringVar(value="0.0 A"),
            "Capacity": tk.StringVar(value="0 mAh"),
            "Altitude": tk.StringVar(value="0.0 m"),
            "Satellite": tk.StringVar(value="0"),
            "GPS": tk.StringVar(value="0.0, 0.0"),
            "Attitude": tk.StringVar(value="P:0 R:0 Y:0"),
            "Mode": tk.StringVar(value="UNKNOWN"),
            "RSSI": tk.StringVar(value="0 dBm"),
            "Link": tk.StringVar(value="0 %")
        }
        
        self.status_var = tk.StringVar(value="Подключение...")
        self.status_color = "orange"

        self.create_widgets()

        # Запуск фонового потока обновления
        self.thread = threading.Thread(target=self.update_data_loop, daemon=True)
        self.thread.start()

    def create_widgets(self):
        # --- Блок телеметрии ---
        telemetry_frame = ttk.LabelFrame(self.root, text="Telemetry Data")
        telemetry_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

        row = 0
        for key, var in self.telemetry_vars.items():
            ttk.Label(telemetry_frame, text=f"{key}:").grid(row=row, column=0, sticky="w", padx=5)
            ttk.Label(telemetry_frame, textvariable=var).grid(row=row, column=1, sticky="e", padx=5)
            row += 1

        # --- Блок управления (RC Channels) ---
        control_frame = ttk.LabelFrame(self.root, text="RC Control (Channels 1-4)")
        control_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")

        self.sliders = []
        channel_names = ["Throttle (1)", "Roll (2)", "Pitch (3)", "Yaw (4)"]
        
        for i in range(4):
            ttk.Label(control_frame, text=channel_names[i]).grid(row=i, column=0, padx=5)
            # Throttle от 1000, остальные центрированы 1500, но слайдер двигаем
            # Важно: command вызывает update_channel при каждом сдвиге
            scale = tk.Scale(control_frame, from_=1000, to=2000, orient="horizontal", length=200,
                             command=lambda val, ch=i: self.update_channel(ch, val))
            scale.set(self.channels[i])
            scale.grid(row=i, column=1, padx=5, pady=5)
            self.sliders.append(scale)
            
        # Кнопки AUX
        aux_frame = ttk.LabelFrame(self.root, text="AUX Channels")
        aux_frame.grid(row=1, column=0, columnspan=2, sticky="ew", padx=10)
        
        self.arm_btn = ttk.Button(aux_frame, text="DISARMED", command=self.toggle_arm)
        self.arm_btn.pack(side="left", padx=20, pady=10)
        
        # --- Статус бар ---
        self.status_label = tk.Label(self.root, textvariable=self.status_var, bg="lightgray", anchor="w")
        self.status_label.grid(row=2, column=0, columnspan=2, sticky="ew")

    def toggle_arm(self):
        # Эмуляция переключателя ARM на 5 канале
        if self.channels[4] < 1500:
            self.channels[4] = 2000
            self.arm_btn.config(text="ARMED (Active)")
        else:
            self.channels[4] = 1000
            self.arm_btn.config(text="DISARMED")
        self.send_rc_command()

    def update_channel(self, channel_index, value):
        self.channels[channel_index] = int(value)
        # Ограничение частоты отправки (чтобы не DDOS-ить сервер)
        current_time = time.time()
        if current_time - self.last_send_time > self.send_interval:
            self.send_rc_command()
            self.last_send_time = current_time

    def send_rc_command(self):
        try:
            url = f"{self.api_url}/api/v1/channels"
            headers = {'Content-Type': 'application/json'}
            payload = {"channels": self.channels}
            
            # ВАЖНО: timeout=0.1, чтобы интерфейс не фризился
            response = requests.post(url, data=json.dumps(payload), headers=headers, timeout=0.1)
            
            if response.status_code == 200:
                self.set_status("Connected (TX OK)", "lightgreen")
            else:
                self.set_status(f"Error TX: {response.status_code}", "salmon")
                
        except requests.exceptions.RequestException:
            # Молча обрабатываем ошибку, не блокируя интерфейс окнами
            self.set_status("Connection Lost (TX)", "red")

    def update_data_loop(self):
        """Фоновый поток получения телеметрии"""
        while self.running:
            try:
                # Тайм-аут важен!
                response = requests.get(f"{self.api_url}/api/v1/telemetry", timeout=0.2)
                
                if response.status_code == 200:
                    data = response.json()
                    # Обновление GUI должно быть в основном потоке
                    self.root.after(0, self.update_gui_labels, data)
                else:
                    self.root.after(0, lambda: self.set_status("Server Error (RX)", "orange"))

            except requests.exceptions.RequestException:
                 self.root.after(0, lambda: self.set_status("Connection Lost (RX)", "red"))
            
            time.sleep(self.interval)

    def update_gui_labels(self, data):
        """Парсинг JSON и обновление переменных"""
        try:
            self.set_status("Connected (Link OK)", "lightgreen")
            
            # Безопасное извлечение данных (если ключа нет, будет N/A)
            self.telemetry_vars["Voltage"].set(f"{data.get('voltage', 0):.1f} V")
            self.telemetry_vars["Current"].set(f"{data.get('current', 0):.1f} A")
            self.telemetry_vars["Altitude"].set(f"{data.get('altitude', 0):.1f} m")
            self.telemetry_vars["Attitude"].set(
                f"P:{data.get('pitch', 0):.0f} R:{data.get('roll', 0):.0f} Y:{data.get('yaw', 0):.0f}"
            )
            
            gps = data.get('gps', {})
            self.telemetry_vars["GPS"].set(f"{gps.get('lat', 0):.4f}, {gps.get('lon', 0):.4f}")
            self.telemetry_vars["Satellite"].set(str(gps.get('satellites', 0)))
            
            # Обработка статистики линка (если есть)
            link = data.get('link_statistics', {})
            uplink_rssi = link.get('uplink_rssi_1', 0)
            lq = link.get('uplink_link_quality', 0)
            self.telemetry_vars["RSSI"].set(f"{uplink_rssi} dBm")
            self.telemetry_vars["Link"].set(f"{lq} %")

        except Exception as e:
            print(f"Error parsing telemetry: {e}")

    def set_status(self, text, color):
        self.status_var.set(f"Status: {text}")
        self.status_label.config(bg=color)

    def on_close(self):
        self.running = False
        self.root.destroy()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CRSF Interface Client")
    parser.add_argument("--api-url", type=str, default="http://localhost:8081", help="API URL")
    parser.add_argument("--interval", type=float, default=0.1, help="Telemetry update interval (sec)")
    
    args = parser.parse_args()

    root = tk.Tk()
    # Задаем размер окна
    root.geometry("600x450")
    
    app = TelemetryApp(root, args.api_url, args.interval)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()