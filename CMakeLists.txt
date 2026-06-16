cmake_minimum_required(VERSION 3.20)
project(TSE VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(tse_core
    src/url.cpp
    src/html.cpp
    src/segmenter.cpp
    src/crypto.cpp
    src/database.cpp
    src/http_client.cpp
    src/crawler.cpp
    src/simple_server.cpp
)

target_include_directories(tse_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tse_core PUBLIC
    SQLite::SQLite3
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads
)

target_compile_options(tse_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(tse
    src/cli.cpp
)
target_link_libraries(tse PRIVATE tse_core)
target_compile_options(tse PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

add_executable(tse_tests
    tests/test_core.cpp
)
target_link_libraries(tse_tests PRIVATE tse_core)
target_compile_options(tse_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME tse_tests COMMAND tse_tests)
