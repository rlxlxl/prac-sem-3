# Сохранение команд в JSON файл

Агент безопасности автоматически сохраняет последние команды из bash_history в JSON файл.

## Конфигурация

В файле `config/agent_config.json` можно настроить:

```json
{
  "output_json_file": "/tmp/security_events.json",
  "max_json_events": 1000
}
```

- `output_json_file` - путь к файлу, куда сохраняются события (по умолчанию: `/tmp/security_events.json`)
- `max_json_events` - максимальное количество событий в файле (по умолчанию: 1000)

## Формат данных

Каждое событие сохраняется в формате JSON на отдельной строке (JSONL формат):

```json
{"timestamp": "2024-01-15T10:30:00Z", "hostname": "macos-host", "source": "bash_history", "event_type": "sudo_command", "severity": "medium", "user": "username", "process": "bash", "command": "sudo su -", "raw_log": "sudo su -"}
```

### Поля события:

- `timestamp` - время события в формате ISO 8601
- `hostname` - имя хоста
- `source` - источник события (`bash_history`, `system_log`, `unified_log`)
- `event_type` - тип события (`sudo_command`, `su_command`, `ssh_command`, `delete_command`, `command_execution`)
- `severity` - уровень серьезности (`low`, `medium`, `high`)
- `user` - имя пользователя
- `process` - процесс (для bash_history всегда `bash`)
- `command` - выполненная команда
- `raw_log` - исходная строка лога

## Просмотр событий

Для просмотра последних команд можно использовать:

```bash
# Просмотр всех событий
cat /tmp/security_events.json

# Просмотр последних 10 команд
tail -10 /tmp/security_events.json

# Просмотр только sudo команд
grep "sudo_command" /tmp/security_events.json

# Форматированный вывод (требует jq)
cat /tmp/security_events.json | jq .
```

## Типы событий

Агент автоматически определяет тип события по команде:

- `sudo_command` - команды с sudo (severity: medium)
- `su_command` - команды переключения пользователя (severity: medium)
- `ssh_command` - SSH команды (severity: low)
- `delete_command` - команды удаления (rm, del) (severity: medium)
- `command_execution` - остальные команды (severity: low)

## Примечания

- Файл обновляется при каждом обнаружении новых команд в bash_history
- Старые события удаляются при превышении `max_json_events`
- Файл сохраняется в формате JSONL (JSON Lines) - каждая строка это отдельный JSON объект
- События также отправляются в СУБД через TCP соединение

