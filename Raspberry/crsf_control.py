#!/usr/bin/env python3
"""
Кроссплатформенный скрипт управления CRSF системой
Заменяет интерактивное меню на управление через аргументы командной строки
"""

import os
import sys
import subprocess
import signal
import time
import argparse
import platform
from pathlib import Path

class CRSFController:
    def __init__(self):
        self.project_path = Path(__file__).parent.absolute()
        self.server_pid = None
        self.api_server_pid = None
        self.api_interpreter_pid = None
        self.api_server_port = 8081
        self.api_target_port = 8082
        self.api_interpreter_port = 8082
        self.api_target_ip = "localhost"
        
    def get_project_path(self):
        """Получить путь к проекту"""
        return str(self.project_path)
    
    def is_process_running(self, pid):
        """Проверить, запущен ли процесс"""
        if pid is None or pid <= 0:
            return False
        try:
            os.kill(pid, 0)
            return True
        except (OSError, ProcessLookupError):
            return False
    
    def find_process_by_name(self, name):
        """Найти процесс по имени"""
        try:
            if platform.system() == "Windows":
                # Windows
                result = subprocess.run(
                    ['tasklist', '/FI', f'IMAGENAME eq {name}'],
                    capture_output=True, text=True, check=False
                )
                if name in result.stdout:
                    # Извлекаем PID из вывода tasklist
                    lines = result.stdout.split('\n')
                    for line in lines:
                        if name in line:
                            parts = line.split()
                            if len(parts) > 1:
                                try:
                                    return int(parts[1])
                                except ValueError:
                                    pass
                return None
            else:
                # Linux/Unix
                result = subprocess.run(
                    ['pgrep', '-f', name],
                    capture_output=True, text=True, check=False
                )
                if result.stdout.strip():
                    pids = result.stdout.strip().split('\n')
                    if pids:
                        return int(pids[0])
                return None
        except Exception:
            return None
    
    def stop_process_by_name(self, name):
        """Остановить процесс по имени"""
        try:
            if platform.system() == "Windows":
                subprocess.run(['taskkill', '/F', '/IM', name], check=False)
            else:
                subprocess.run(['pkill', '-f', name], check=False)
            time.sleep(1)
        except Exception:
            pass
    
    def stop_process(self, pid, name, process_name=""):
        """Остановить процесс по PID или имени"""
        # Сначала пытаемся остановить по PID
        if pid and self.is_process_running(pid):
            print(f"Остановка {name} (PID: {pid})...")
            try:
                os.kill(pid, signal.SIGTERM)
                time.sleep(1)
                if self.is_process_running(pid):
                    os.kill(pid, signal.SIGKILL)
            except Exception as e:
                print(f"Ошибка при остановке процесса: {e}")
            pid = None
            print(f"{name} остановлен.")
            return
        
        # Если PID не работает, пытаемся найти по имени
        if process_name:
            found_pid = self.find_process_by_name(process_name)
            if found_pid:
                print(f"Остановка {name} (PID: {found_pid})...")
                try:
                    os.kill(found_pid, signal.SIGTERM)
                    time.sleep(1)
                    if self.is_process_running(found_pid):
                        os.kill(found_pid, signal.SIGKILL)
                except Exception as e:
                    print(f"Ошибка при остановке процесса: {e}")
                print(f"{name} остановлен.")
                return
        
        print(f"{name} не запущен.")
    
    def execute_command(self, cmd, wait=True, shell=True):
        """Выполнить команду"""
        print(f"\n>>> Выполняется: {cmd}\n")
        try:
            if wait:
                result = subprocess.run(cmd, shell=shell, check=False)
                if result.returncode != 0:
                    print(f"\nОшибка выполнения команды. Код возврата: {result.returncode}")
                return result.returncode
            else:
                subprocess.Popen(cmd, shell=shell)
                return 0
        except Exception as e:
            print(f"Ошибка выполнения команды: {e}")
            return 1
    
    def start_process(self, cmd, background=True):
        """Запустить процесс"""
        try:
            if background:
                if platform.system() == "Windows":
                    # Windows
                    subprocess.Popen(cmd, shell=True, 
                                   creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
                else:
                    # Linux/Unix
                    process = subprocess.Popen(
                        cmd, shell=True,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                        preexec_fn=os.setsid if hasattr(os, 'setsid') else None
                    )
                    time.sleep(0.5)
                    return process.pid
            else:
                process = subprocess.Popen(cmd, shell=True)
                return process.pid
        except Exception as e:
            print(f"Ошибка запуска процесса: {e}")
            return None
    
    # ========== Команды пересборки ==========
    
    def rebuild_all(self):
        """Пересобрать все"""
        cmd = f"cd {self.get_project_path()} && make clean && make"
        return self.execute_command(cmd)
    
    def rebuild_main(self):
        """Пересобрать основное приложение"""
        cmd = f"cd {self.get_project_path()} && make crsf_io_rpi"
        return self.execute_command(cmd)
    
    def rebuild_api_server(self):
        """Пересобрать API сервер"""
        cmd = f"cd {self.get_project_path()} && make crsf_api_server"
        return self.execute_command(cmd)
    
    def rebuild_api_interpreter(self):
        """Пересобрать API интерпретатор"""
        cmd = f"cd {self.get_project_path()} && make crsf_api_interpreter"
        return self.execute_command(cmd)
    
    def rebuild_pybind(self):
        """Пересобрать Python bindings"""
        cmd = f"cd {self.get_project_path()}/pybind && python3 build_lib.py build_ext --inplace"
        return self.execute_command(cmd)
    
    def clean_build(self):
        """Очистить артефакты сборки"""
        cmd = f"cd {self.get_project_path()} && make clean"
        return self.execute_command(cmd)
    
    # ========== Команды сервера (без API) ==========
    
    def restart_server(self):
        """Перезапустить сервер"""
        self.stop_process(self.server_pid, "Сервер", "crsf_io_rpi")
        
        exe_path = os.path.join(self.get_project_path(), "crsf_io_rpi")
        if not os.path.exists(exe_path):
            print(f"Ошибка: файл {exe_path} не найден!")
            print("Сначала пересоберите проект (команда rebuild).")
            return 1
        
        cmd = f"cd {self.get_project_path()} && sudo ./crsf_io_rpi"
        print("Запуск сервера (требуются права sudo)...")
        pid = self.start_process(cmd)
        if pid:
            self.server_pid = pid
            print(f"Сервер запущен (PID: {pid})")
        else:
            print("Ошибка запуска сервера")
            return 1
        return 0
    
    def stop_server(self):
        """Остановить сервер"""
        self.stop_process(self.server_pid, "Сервер", "crsf_io_rpi")
        self.server_pid = None
    
    def start_interface(self, use_api=False):
        """Запустить интерфейс"""
        existing_pid = self.find_process_by_name("crsf_realtime_interface.py")
        if existing_pid:
            print(f"Интерфейс уже запущен (PID: {existing_pid})")
            return 0
        
        cmd = f"cd {self.get_project_path()} && python3 crsf_realtime_interface.py"
        if use_api:
            cmd += " --api"
        
        pid = self.start_process(cmd, background=False)
        if pid:
            print(f"Интерфейс запущен (PID: {pid})")
        else:
            print("Ошибка запуска интерфейса")
            return 1
        return 0
    
    # ========== Команды API сервера ==========
    
    def restart_api_server(self, port=None, target_ip=None, target_port=None):
        """Перезапустить API сервер"""
        if port:
            self.api_server_port = port
        if target_ip:
            self.api_target_ip = target_ip
        if target_port:
            self.api_target_port = target_port
        
        self.stop_process(self.api_server_pid, "API Сервер", "crsf_api_server")
        
        exe_path = os.path.join(self.get_project_path(), "crsf_api_server")
        if not os.path.exists(exe_path):
            print(f"Ошибка: файл {exe_path} не найден!")
            print("Сначала пересоберите проект (команда rebuild).")
            return 1
        
        cmd = f"cd {self.get_project_path()} && ./crsf_api_server {self.api_server_port} {self.api_target_ip} {self.api_target_port}"
        pid = self.start_process(cmd)
        if pid:
            self.api_server_pid = pid
            print(f"API Сервер запущен (PID: {pid})")
            print(f"  Слушающий порт: {self.api_server_port}")
            print(f"  Целевой IP: {self.api_target_ip}")
            print(f"  Целевой порт: {self.api_target_port}")
        else:
            print("Ошибка запуска API сервера")
            return 1
        return 0
    
    def stop_api_server(self):
        """Остановить API сервер"""
        self.stop_process(self.api_server_pid, "API Сервер", "crsf_api_server")
        self.api_server_pid = None
    
    # ========== Команды API интерпретатора ==========
    
    def restart_api_interpreter(self, port=None):
        """Перезапустить API интерпретатор"""
        if port:
            self.api_interpreter_port = port
        
        self.stop_process(self.api_interpreter_pid, "API Интерпретатор", "crsf_api_interpreter")
        
        exe_path = os.path.join(self.get_project_path(), "crsf_api_interpreter")
        if not os.path.exists(exe_path):
            print(f"Ошибка: файл {exe_path} не найден!")
            print("Сначала пересоберите проект (команда rebuild).")
            return 1
        
        cmd = f"cd {self.get_project_path()} && ./crsf_api_interpreter {self.api_interpreter_port}"
        pid = self.start_process(cmd)
        if pid:
            self.api_interpreter_pid = pid
            print(f"API Интерпретатор запущен (PID: {pid})")
            print(f"  Порт: {self.api_interpreter_port}")
        else:
            print("Ошибка запуска API интерпретатора")
            return 1
        return 0
    
    def stop_api_interpreter(self):
        """Остановить API интерпретатор"""
        self.stop_process(self.api_interpreter_pid, "API Интерпретатор", "crsf_api_interpreter")
        self.api_interpreter_pid = None
    
    def kill_process_on_port(self, port):
        """Завершить процессы на порту"""
        if port <= 0:
            print("Неверный номер порта")
            return 1
        
        try:
            if platform.system() == "Windows":
                # Windows
                cmd = f'netstat -ano | findstr :{port}'
                result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
                if result.stdout:
                    # Извлекаем PID и убиваем процесс
                    for line in result.stdout.split('\n'):
                        parts = line.split()
                        if len(parts) > 4:
                            try:
                                pid = int(parts[-1])
                                subprocess.run(['taskkill', '/F', '/PID', str(pid)], check=False)
                            except (ValueError, IndexError):
                                pass
            else:
                # Linux/Unix
                cmd = f"sudo fuser -k {port}/tcp"
                self.execute_command(cmd)
            print(f"Попытка завершить процессы на порту {port}.")
            return 0
        except Exception as e:
            print(f"Ошибка при завершении процессов на порту: {e}")
            return 1


def main():
    parser = argparse.ArgumentParser(
        description='Кроссплатформенное управление CRSF системой',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры использования:

  Пересборка:
    %(prog)s rebuild all
    %(prog)s rebuild main
    %(prog)s rebuild api-server
    %(prog)s rebuild api-interpreter
    %(prog)s rebuild pybind
    %(prog)s rebuild clean

  Основное приложение (crsf_io_rpi):
    %(prog)s app start
    %(prog)s app stop
    %(prog)s server start   # алиас для app start
    %(prog)s server stop    # алиас для app stop

  Интерфейс:
    %(prog)s interface start
    %(prog)s interface start --api

  Режим через API (сервер):
    %(prog)s api-server start --port 8081 --target-ip localhost --target-port 8082
    %(prog)s api-server stop

  Режим через API (клиент):
    %(prog)s api-interpreter start --port 8082
    %(prog)s api-interpreter stop

  Утилиты:
    %(prog)s kill-port 8081
        """
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Команда')
    
    # Парсер для rebuild
    rebuild_parser = subparsers.add_parser('rebuild', help='Пересборка проекта')
    rebuild_parser.add_argument('target', choices=['all', 'main', 'api-server', 'api-interpreter', 'pybind', 'clean'],
                               help='Цель пересборки')
    
    # Парсер для app (основное приложение crsf_io_rpi)
    app_parser = subparsers.add_parser('app', help='Управление основным приложением (crsf_io_rpi)')
    app_parser.add_argument('action', choices=['start', 'stop'], help='Действие')
    
    # Парсер для server (алиас для app)
    server_parser = subparsers.add_parser('server', help='Управление сервером (без API) - алиас для app')
    server_parser.add_argument('action', choices=['start', 'stop'], help='Действие')
    
    # Парсер для interface
    interface_parser = subparsers.add_parser('interface', help='Управление интерфейсом')
    interface_parser.add_argument('action', choices=['start'], help='Действие')
    interface_parser.add_argument('--api', action='store_true', help='Использовать API режим')
    
    # Парсер для api-server
    api_server_parser = subparsers.add_parser('api-server', help='Управление API сервером')
    api_server_parser.add_argument('action', choices=['start', 'stop'], help='Действие')
    api_server_parser.add_argument('--port', type=int, help='Слушающий порт (по умолчанию: 8081)')
    api_server_parser.add_argument('--target-ip', help='Целевой IP (по умолчанию: localhost)')
    api_server_parser.add_argument('--target-port', type=int, help='Целевой порт (по умолчанию: 8082)')
    
    # Парсер для api-interpreter
    api_interpreter_parser = subparsers.add_parser('api-interpreter', help='Управление API интерпретатором')
    api_interpreter_parser.add_argument('action', choices=['start', 'stop'], help='Действие')
    api_interpreter_parser.add_argument('--port', type=int, help='Порт интерпретатора (по умолчанию: 8082)')
    
    # Парсер для kill-port
    kill_port_parser = subparsers.add_parser('kill-port', help='Завершить процессы на порту')
    kill_port_parser.add_argument('port', type=int, help='Номер порта')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    controller = CRSFController()
    
    try:
        if args.command == 'app' or args.command == 'server':
            if args.action == 'start':
                return controller.restart_server()
            elif args.action == 'stop':
                controller.stop_server()
                return 0
        
        elif args.command == 'rebuild':
            if args.target == 'all':
                return controller.rebuild_all()
            elif args.target == 'main':
                return controller.rebuild_main()
            elif args.target == 'api-server':
                return controller.rebuild_api_server()
            elif args.target == 'api-interpreter':
                return controller.rebuild_api_interpreter()
            elif args.target == 'pybind':
                return controller.rebuild_pybind()
            elif args.target == 'clean':
                return controller.clean_build()
        
        elif args.command == 'interface':
            if args.action == 'start':
                return controller.start_interface(use_api=args.api)
        
        elif args.command == 'api-server':
            if args.action == 'start':
                return controller.restart_api_server(
                    port=args.port,
                    target_ip=args.target_ip,
                    target_port=args.target_port
                )
            elif args.action == 'stop':
                controller.stop_api_server()
                return 0
        
        elif args.command == 'api-interpreter':
            if args.action == 'start':
                return controller.restart_api_interpreter(port=args.port)
            elif args.action == 'stop':
                controller.stop_api_interpreter()
                return 0
        
        elif args.command == 'kill-port':
            return controller.kill_process_on_port(args.port)
        
    except KeyboardInterrupt:
        print("\n\nПрервано пользователем")
        return 1
    except Exception as e:
        print(f"Ошибка: {e}")
        return 1
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
