cmake_minimum_required(VERSION 3.10)
project(no_sql_dbms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Original standalone executable
add_executable(no_sql_dbms
    src/main.cpp
    src/collection.cpp
    src/db.cpp
)

# Database server
add_executable(db_server
    src/server_main.cpp
    src/server.cpp
    src/collection.cpp
    src/db.cpp
)

# Database client
add_executable(db_client
    src/client.cpp
    src/collection.cpp
    src/db.cpp
)