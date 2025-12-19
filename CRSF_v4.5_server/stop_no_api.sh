#!/bin/bash
# Скрипт остановки CRSF системы без API
# Использование: ./stop_no_api.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🛑 Остановка CRSF системы без API..."

python3 crsf_control.py app stop 2>/dev/null || true
pkill -f crsf_realtime_interface.py 2>/dev/null || true

sleep 1

echo "✅ Система остановлена"
