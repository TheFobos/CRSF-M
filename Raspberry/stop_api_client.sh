#!/bin/bash
# Скрипт остановки API клиента
# Использование: ./stop_api_client.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🛑 Остановка API клиента..."

python3 crsf_control.py app stop 2>/dev/null || true
python3 crsf_control.py api-interpreter stop 2>/dev/null || true
pkill -f crsf_realtime_interface.py 2>/dev/null || true

sleep 1

echo "✅ API клиент остановлен"
