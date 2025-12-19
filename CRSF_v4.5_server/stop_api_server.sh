#!/bin/bash
# Скрипт остановки API сервера
# Использование: ./stop_api_server.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🛑 Остановка API сервера..."

python3 crsf_control.py api-server stop 2>/dev/null || true

sleep 1

echo "✅ API сервер остановлен"
