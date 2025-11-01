#!/bin/bash

# Скрипт для запуска experiment-service

set -e

echo "🚀 Starting Experiment Service..."

# Проверка наличия docker-compose
if command -v docker-compose &> /dev/null || command -v docker &> /dev/null; then
    echo "📦 Using Docker Compose to start database..."

    # Запуск только БД если сервис запускается локально
    if [ "$1" = "local" ]; then
        echo "Starting only PostgreSQL database..."
        docker-compose up -d postgres
        echo "✅ Database started. You can now run: python main.py"
        exit 0
    fi

    # Полный запуск через docker-compose
    echo "Starting all services (database + service)..."
    docker-compose up -d
    echo "✅ Services started!"
    echo "📊 View logs: docker-compose logs -f experiment-service"
    echo "🌐 Service available at: http://localhost:8002"
    echo "🛑 Stop services: docker-compose down"
else
    echo "⚠️  Docker Compose not found. Make sure PostgreSQL is running manually."
    echo "Then run: python main.py"
fi

