#!/bin/bash

# Скрипт для настройки zsh для немедленной записи истории
# Это гарантирует, что команды будут записываться в файл истории сразу

echo "Настройка zsh для немедленной записи истории..."

ZSH_RC="$HOME/.zshrc"

# Проверяем, существует ли файл .zshrc
if [ ! -f "$ZSH_RC" ]; then
    echo "Создаем файл .zshrc..."
    touch "$ZSH_RC"
fi

# Проверяем, есть ли уже настройки истории
if grep -q "INC_APPEND_HISTORY" "$ZSH_RC"; then
    echo "Настройки истории уже присутствуют в .zshrc"
else
    echo "" >> "$ZSH_RC"
    echo "# Настройки для немедленной записи истории (для security_agent)" >> "$ZSH_RC"
    echo "setopt INC_APPEND_HISTORY     # Записывать историю сразу, не ждать выхода из сессии" >> "$ZSH_RC"
    echo "setopt SHARE_HISTORY          # Делиться историей между сессиями" >> "$ZSH_RC"
    echo "setopt HIST_FCNTL_LOCK        # Использовать файловые блокировки для истории" >> "$ZSH_RC"
    echo "setopt HIST_IGNORE_DUPS       # Игнорировать дубликаты" >> "$ZSH_RC"
    echo "setopt HIST_IGNORE_SPACE      # Игнорировать команды, начинающиеся с пробела" >> "$ZSH_RC"
    echo "" >> "$ZSH_RC"
    echo "Настройки добавлены в .zshrc"
fi

echo ""
echo "Для применения изменений выполните:"
echo "  source ~/.zshrc"
echo ""
echo "Или перезапустите терминал."


