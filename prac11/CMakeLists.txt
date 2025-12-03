cmake_minimum_required(VERSION 3.10)
project(no_sql_dbms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

add_executable(no_sql_dbms
    src/main.cpp
    src/collection.cpp
    src/db.cpp
)