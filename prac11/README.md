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

## Примеры использования
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