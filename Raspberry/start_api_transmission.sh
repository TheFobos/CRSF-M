#!/bin/bash
# Скрипт для запуска передачи по API
# Использование:
#   На ведомом узле: ./start_api_transmission.sh interpreter [порт]
#   На ведущем узле: ./start_api_transmission.sh server [порт] [IP_ведомого] [порт_ведомого]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

case "$1" in
    interpreter)
        PORT=${2:-8082}
        echo "🚀 Запуск API интерпретатора на порту $PORT"
        echo "📝 Команды будут записываться в /tmp/crsf_command.txt"
        echo ""
        echo "⚠️  Убедитесь, что основное приложение crsf_io_rpi запущено!"
        echo ""
        ./crsf_api_interpreter "$PORT"
        ;;
    server)
        PORT=${2:-8081}
        TARGET_HOST=${3:-localhost}
        TARGET_PORT=${4:-8082}
        echo "🚀 Запуск API сервера на порту $PORT"
        echo "🎯 Целевой узел: $TARGET_HOST:$TARGET_PORT"
        echo ""
        ./crsf_api_server "$PORT" "$TARGET_HOST" "$TARGET_PORT"
        ;;
    *)
        echo "Использование:"
        echo "  На ведомом узле:"
        echo "    $0 interpreter [порт]"
        echo "    Пример: $0 interpreter 8082"
        echo ""
        echo "  На ведущем узле:"
        echo "    $0 server [порт] [IP_ведомого] [порт_ведомого]"
        echo "    Пример: $0 server 8081 192.168.1.100 8082"
        echo ""
        echo "Параметры по умолчанию:"
        echo "  API Server порт: 8081"
        echo "  API Interpreter порт: 8082"
        exit 1
        ;;
esac

