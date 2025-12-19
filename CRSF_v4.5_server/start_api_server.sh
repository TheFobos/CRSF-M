#!/bin/bash
# Скрипт запуска API сервера
# Использование: ./start_api_server.sh [порт] [IP_ведомого] [порт_ведомого]
# Пример: ./start_api_server.sh 8081 192.168.1.100 8082

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Параметры по умолчанию
PORT=${1:-8081}
TARGET_IP=${2:-localhost}
TARGET_PORT=${3:-8082}

echo "╔════════════════════════════════════════╗"
echo "║   Запуск API Сервера                 ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "📡 Конфигурация:"
echo "  Слушающий порт: $PORT"
echo "  Целевой IP:     $TARGET_IP"
echo "  Целевой порт:   $TARGET_PORT"
echo ""

# Проверяем наличие crsf_control.py
if [ ! -f "crsf_control.py" ]; then
    echo "❌ Ошибка: файл crsf_control.py не найден!"
    exit 1
fi

# Проверяем наличие API сервера
if [ ! -f "crsf_api_server" ]; then
    echo "⚠️  API сервер crsf_api_server не найден."
    echo "📦 Пересобираем API сервер..."
    python3 crsf_control.py rebuild api-server
    if [ ! -f "crsf_api_server" ]; then
        echo "❌ Ошибка: не удалось собрать crsf_api_server"
        exit 1
    fi
fi

# Останавливаем существующий API сервер
echo "🛑 Остановка существующего API сервера..."
python3 crsf_control.py api-server stop 2>/dev/null || true
sleep 1

# Проверяем, не занят ли порт
if command -v ss >/dev/null 2>&1; then
    if ss -tlnp | grep -q ":$PORT "; then
        echo "⚠️  Порт $PORT уже занят!"
        echo "💡 Попробуйте освободить порт: python3 crsf_control.py kill-port $PORT"
        read -p "Продолжить? (y/n): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
fi

# Запускаем API сервер
echo ""
echo "🚀 Запуск API сервера..."
python3 crsf_control.py api-server start --port "$PORT" --target-ip "$TARGET_IP" --target-port "$TARGET_PORT"

echo ""
echo "✅ API сервер запущен!"
echo ""
echo "📋 Конфигурация:"
echo "  - Слушающий порт: $PORT"
echo "  - Целевой узел: $TARGET_IP:$TARGET_PORT"
echo ""
echo "💡 Для остановки используйте: ./stop_api_server.sh"
echo "   или: python3 crsf_control.py api-server stop"
echo ""
echo "📝 Пример использования:"
echo "   curl -X POST http://localhost:$PORT/api/command/setChannel \\"
echo "     -H \"Content-Type: application/json\" \\"
echo "     -d '{\"channel\":1,\"value\":1500}'"
