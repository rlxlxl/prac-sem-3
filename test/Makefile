CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude -pthread

# Цели для сборки
all: build/no_sql_dbms build/db_server build/db_client build/security_agent

# Старое CLI приложение
build/no_sql_dbms: src/main.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

# Сервер БД
build/db_server: src/server.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

# Клиент БД
build/db_client: src/client.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

# Агент безопасности
build/security_agent: src/security_agent.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f build/no_sql_dbms build/db_server build/db_client build/security_agent
	rm -rf build

.PHONY: all clean

