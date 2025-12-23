#!/bin/bash

# Скрипт для запуска SIEM веб-приложения

echo "=== SIEM Web Application ==="
echo ""

# Проверяем наличие Python
if ! command -v python3 &> /dev/null; then
    echo "Ошибка: Python 3 не найден"
    exit 1
fi

# Создаем виртуальное окружение, если его нет
if [ ! -d "venv" ]; then
    echo "Создание виртуального окружения..."
    python3 -m venv venv
fi

# Активируем виртуальное окружение
source venv/bin/activate

# Проверяем наличие зависимостей
if ! python -c "import flask" 2>/dev/null; then
    echo "Установка зависимостей..."
    pip install -r requirements.txt
fi

# Проверяем переменные окружения
if [ -z "$SIEM_USERNAME" ]; then
    export SIEM_USERNAME=admin
    echo "Используется имя пользователя по умолчанию: admin"
fi

if [ -z "$SIEM_PASSWORD" ]; then
    export SIEM_PASSWORD=admin123
    echo "Используется пароль по умолчанию: admin123"
fi

if [ -z "$DB_HOST" ]; then
    export DB_HOST=localhost
fi

if [ -z "$DB_PORT" ]; then
    export DB_PORT=8080
fi

if [ -z "$WEB_PORT" ]; then
    export WEB_PORT=5001
fi

echo ""
echo "Конфигурация:"
echo "  Username: $SIEM_USERNAME"
echo "  DB Host: $DB_HOST"
echo "  DB Port: $DB_PORT"
echo "  Web Port: $WEB_PORT"
echo ""
echo "Запуск веб-сервера на http://localhost:$WEB_PORT"
echo ""

# Запускаем приложение
python app.py

