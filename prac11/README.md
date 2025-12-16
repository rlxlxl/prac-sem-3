# no_sql_dbms

## Требования
- C++17
- CMake
- Скачайте single-header nlohmann/json.hpp и поместите в `third_party/json.hpp`.

## Сборка
mkdir build
cd build
cmake ..
make

После сборки будут созданы три исполняемых файла:
- `no_sql_dbms` - оригинальная локальная версия
- `db_server` - сервер БД для сетевого доступа
- `db_client` - клиент для подключения к серверу

## Использование локальной версии

# Вставка
./no_sql_dbms ./data insert '{"name":"Alice","age":25,"city":"London"}'

# Поиск (implicit AND)
./no_sql_dbms ./data find '{"name":"Alice","age":25}'

# OR
./no_sql_dbms ./data find '{"$or":[{"age":25},{"city":"Paris"}]}'

# $gt, $lt, $like, $in
./no_sql_dbms ./data find '{"age":{"$gt":20}}'
./no_sql_dbms ./data find '{"name":{"$like":"Ali%"}}'
./no_sql_dbms ./data find '{"city":{"$in":["London","Paris"]}}'

# Удаление
./no_sql_dbms ./data delete '{"age":25}'
./no_sql_dbms ./data delete '{"name":{"$like":"A%"}}'

# Создать индекс (stub)
./no_sql_dbms ./data create_index age

## Использование сетевого интерфейса

### Запуск сервера

```bash
./db_server --db-dir build/my_database --port 8080
```

Сервер запустится и будет ожидать подключений на указанном порту.

**Примечание:** Если порт занят (например, 8080 может быть занят Docker), используйте другой порт:
```bash
./db_server --db-dir build/my_database --port 9000
```

Проверить, занят ли порт:
```bash
lsof -i :8080
```

### Подключение клиента

```bash
./db_client --host localhost --port 8080 --database my_database
```

**Важно:** Порт клиента должен совпадать с портом сервера!

После подключения откроется интерактивный режим (REPL), где можно вводить команды:

```
> INSERT users{'name': 'Alice', 'age': 25}
> FIND users{'age': {'$gt': 20}}
> DELETE users{'name': 'Alice'}
```

### Формат команд клиента

- **INSERT** `collection{...}` - вставка документа(ов)
- **FIND** `collection{...}` - поиск документов по запросу
- **DELETE** `collection{...}` - удаление документов по запросу

Примеры:
```
> INSERT users{'name': 'Bob', 'age': 30, 'city': 'Paris'}
> FIND users{'age': {'$gt': 25}}
> DELETE users{'name': 'Bob'}
```

### Особенности

- Сервер поддерживает множественные одновременные подключения (минимум 5 клиентов)
- Операции записи (INSERT, DELETE) защищены блокировками на уровне базы данных
- Операции чтения (FIND) выполняются без блокировок для лучшей производительности
- Каждая база данных имеет свой мьютекс для обеспечения потокобезопасности