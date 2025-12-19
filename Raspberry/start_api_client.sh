#!/bin/bash
# Скрипт запуска API клиента (интерпретатор + основное приложение + GUI)
# Использование: ./start_api_client.sh [порт_интерпретатора]
# Пример: ./start_api_client.sh 8082

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Параметры по умолчанию
INTERPRETER_PORT=${1:-8082}

echo "╔════════════════════════════════════════╗"
echo "║   Запуск API Клиента                 ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "📡 Конфигурация:"
echo "  Порт интерпретатора: $INTERPRETER_PORT"
echo ""

# Проверяем наличие crsf_control.py
if [ ! -f "crsf_control.py" ]; then
    echo "❌ Ошибка: файл crsf_control.py не найден!"
    exit 1
fi

# Проверяем наличие основного приложения
if [ ! -f "crsf_io_rpi" ]; then
    echo "⚠️  Основное приложение crsf_io_rpi не найдено."
    echo "📦 Пересобираем проект..."
    python3 crsf_control.py rebuild main
    if [ ! -f "crsf_io_rpi" ]; then
        echo "❌ Ошибка: не удалось собрать crsf_io_rpi"
        exit 1
    fi
fi

# Проверяем наличие API интерпретатора
if [ ! -f "crsf_api_interpreter" ]; then
    echo "⚠️  API интерпретатор crsf_api_interpreter не найден."
    echo "📦 Пересобираем API интерпретатор..."
    python3 crsf_control.py rebuild api-interpreter
    if [ ! -f "crsf_api_interpreter" ]; then
        echo "❌ Ошибка: не удалось собрать crsf_api_interpreter"
        exit 1
    fi
fi

# Останавливаем существующие процессы
echo "🛑 Остановка существующих процессов..."
python3 crsf_control.py app stop 2>/dev/null || true
python3 crsf_control.py api-interpreter stop 2>/dev/null || true
pkill -f crsf_realtime_interface.py 2>/dev/null || true
sleep 1

# Проверяем, не занят ли порт
if command -v ss >/dev/null 2>&1; then
    if ss -tlnp | grep -q ":$INTERPRETER_PORT "; then
        echo "⚠️  Порт $INTERPRETER_PORT уже занят!"
        echo "💡 Попробуйте освободить порт: python3 crsf_control.py kill-port $INTERPRETER_PORT"
        read -p "Продолжить? (y/n): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
fi

# Запускаем основное приложение
echo ""
echo "🚀 Запуск основного приложения (crsf_io_rpi)..."
python3 crsf_control.py app start

# Даем время на запуск
sleep 2

# Запускаем API интерпретатор
echo ""
echo "🚀 Запуск API интерпретатора..."
python3 crsf_control.py api-interpreter start --port "$INTERPRETER_PORT"

# Даем время на запуск
sleep 2

# Запускаем GUI интерфейс с поддержкой API
echo ""
echo "🖥️  Запуск графического интерфейса (с API)..."
python3 crsf_control.py interface start --api

echo ""
echo "✅ Система запущена!"
echo ""
echo "📋 Запущенные процессы:"
echo "  - crsf_io_rpi (основное приложение)"
echo "  - crsf_api_interpreter (порт: $INTERPRETER_PORT)"
echo "  - crsf_realtime_interface.py (GUI с API)"
echo ""
echo "💡 Для остановки используйте: ./stop_api_client.sh"
echo "   или:"
echo "     python3 crsf_control.py app stop"
echo "     python3 crsf_control.py api-interpreter stop"
echo ""
echo "📝 Команды будут записываться в /tmp/crsf_command.txt"
echo "   Проверить: tail -f /tmp/crsf_command.txt"
