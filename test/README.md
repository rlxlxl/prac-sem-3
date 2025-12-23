# NoSQL DBMS - Сетевая версия

Документно-ориентированная СУБД с сетевым интерфейсом.

## Сборка

```bash
make
```

Создаст три исполняемых файла:
- `build/no_sql_dbms` - локальное CLI приложение
- `build/db_server` - сервер БД
- `build/db_client` - клиент БД

## Использование

### Запуск сервера

```bash
./build/db_server [port]
```

По умолчанию сервер запускается на порту 8080.

### Запуск клиента

```bash
./build/db_client --host localhost --port 8080 --database my_database
```

### Команды клиента

В интерактивном режиме доступны следующие команды:

- `INSERT <collection> <json>` - вставить документ
- `FIND <collection> <json_query>` - найти документы
- `DELETE <collection> <json_query>` - удалить документы
- `CREATE_INDEX <collection> <field>` - создать индекс
- `exit` или `quit` - выйти

### Примеры

```bash
# Вставка документа
> INSERT users {"name": "Alice", "age": 25, "city": "London"}

# Поиск документов
> FIND users {"age": 25}
> FIND users {"age": {"$gt": 20}}
> FIND users {"$or": [{"age": 25}, {"city": "Paris"}]}

# Удаление документов
> DELETE users {"age": 25}

# Создание индекса
> CREATE_INDEX users age
```

## Архитектура

- **Сервер**: Многопоточный TCP-сервер, обрабатывает множественные подключения
- **Клиент**: Интерактивное приложение для работы с БД
- **Блокировки**: Используются shared_mutex для потокобезопасности операций записи/чтения

## Формат сетевых сообщений

### Запрос
```json
{
  "database": "my_database",
  "operation": "insert|find|delete|create_index",
  "collection": "users",
  "data": {...},  // для insert
  "query": {...}, // для find/delete
  "field": "age"  // для create_index
}
```

### Ответ
```json
{
  "status": "success|error",
  "message": "Описание результата",
  "data": [...],  // массив документов
  "count": 0      // количество документов
}
```

