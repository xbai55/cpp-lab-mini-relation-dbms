cmake_minimum_required(VERSION 3.16)
project(mini_relation_dbms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(include)

add_library(mini_db_core
    src/common/protocol.cpp
    src/storage/bptree.cpp
    src/storage/database.cpp
    src/sql/parser.cpp
    src/sql/executor.cpp
)

add_executable(mini_db_server src/server/main.cpp)
target_link_libraries(mini_db_server PRIVATE mini_db_core)

add_executable(mini_db_client src/client/main.cpp)
target_link_libraries(mini_db_client PRIVATE mini_db_core)

if(WIN32)
    target_link_libraries(mini_db_core PUBLIC ws2_32)
endif()

add_executable(mini_db_tests tests/test_main.cpp)
target_link_libraries(mini_db_tests PRIVATE mini_db_core)
