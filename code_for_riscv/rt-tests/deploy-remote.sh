#!/bin/bash
# deploy-riscv.sh - отправка и компиляция на RISC-V одноплатнике Pi
# Конфигурация под ваш одноплатник

# === НАСТРОЙКИ ===
REMOTE_USER="root"
REMOTE_HOST="192.168.213.186"
REMOTE_DIR="/root/rt-tests"
SOURCE_FILES="*.c *.h"       # Какие файлы отправлять
TARGET_NAME="lichee_example"

# Проверка аргументов
if [ $# -eq 0 ]; then
    echo "Использование: $0 <файлы для отправки>"
    echo "Пример: $0 lichee_example.c"
    echo "Или: $0 *.c *.h"
    echo ""
    echo "Доступные команды:"
    echo "  $0 <файлы>      - отправить и скомпилировать"
    echo "  $0 clean        - удалить программу на одноплатнике"
    echo "  $0 run          - только запустить программу"
    echo "  $0 list         - показать файлы на одноплатнике"
    exit 1
fi

# Специальные команды
case "$1" in
    "clean")
        echo "🧹 Очистка на одноплатнике..."
        ssh $REMOTE_USER@$REMOTE_HOST "cd $REMOTE_DIR && rm -f $TARGET_NAME *.o"
        echo "✅ Очистка завершена"
        exit 0
        ;;
    "run")
        echo "▶️  Запуск программы на одноплатнике..."
        ssh $REMOTE_USER@$REMOTE_HOST "cd $REMOTE_DIR && sudo ./$TARGET_NAME"
        exit 0
        ;;
    "list")
        echo "📁 Файлы на одноплатнике в $REMOTE_DIR:"
        ssh $REMOTE_USER@$REMOTE_HOST "cd $REMOTE_DIR && ls -la"
        exit 0
        ;;
esac

echo "🚀 Отправка файлов на RISC-V одноплатник..."
echo "📡 Адрес: $REMOTE_USER@$REMOTE_HOST"
echo "📁 Директория: $REMOTE_DIR"

# Создаем директорию на удаленной машине
ssh $REMOTE_USER@$REMOTE_HOST "mkdir -p $REMOTE_DIR"

# Отправляем файлы
for file in "$@"; do
    if [ -f "$file" ]; then
        echo "📤 Отправка: $file"
        scp "$file" $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR/
    else
        echo "⚠️  Файл не найден: $file"
    fi
done

echo "🔧 Компиляция на RISC-V одноплатнике..."
echo "💡 Используется команда: gcc lichee_example.c -o lichee_example -lgpiod"

# Компилируем на удаленной машине с библиотекой gpiod
ssh $REMOTE_USER@$REMOTE_HOST "
    cd $REMOTE_DIR
    echo '📦 Файлы в директории /tmp:'
    ls -la *.c *.h 2>/dev/null || echo '   (нет C файлов)'
    
    echo '🛠️  Компиляция с библиотекой gpiod...'
    
    # Основная команда компиляции
    if [ -f 'lichee_example.c' ]; then
        gcc lichee_example.c -o $TARGET_NAME -lgpiod 2>&1
    else
        # Если файл называется иначе
        C_FILE=\$(ls *.c | head -1)
        if [ -n \"\$C_FILE\" ]; then
            echo \"📄 Используем файл: \$C_FILE\"
            gcc \"\$C_FILE\" -o $TARGET_NAME -lgpiod 2>&1
        else
            echo '❌ Не найден ни один .c файл!'
            exit 1
        fi
    fi
    
    COMPILE_STATUS=\$?
"
