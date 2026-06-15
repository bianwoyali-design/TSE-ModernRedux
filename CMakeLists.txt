cmake_minimum_required(VERSION 3.20)
project(TSE VERSION 2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# ------------------------------------------------------------------
# Pre-generate flex lexers (run manually before build if .l files change:
#   flex -o tse/hlink/lex.hlink.c tse/hlink/hlink.l
#   flex -o tse/uri/lex.uri.c tse/uri/uri.l
# )
# ------------------------------------------------------------------

# === C support libraries ===

# stack library
add_library(tse_stack STATIC
    tse/stack/stack.c
)
target_include_directories(tse_stack PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/stack
)

# misc library
add_library(tse_misc STATIC
    tse/lib/misc.c
)
target_include_directories(tse_misc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/lib
)

# Flex-generated URI lexer (C)
add_library(tse_uri_lex STATIC
    tse/uri/lex.uri.c
)
target_include_directories(tse_uri_lex PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/uri
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/lib
)
# Flex-generated code needs relaxed warnings
target_compile_options(tse_uri_lex PRIVATE -w)

# Flex-generated hlink lexer (C)
add_library(tse_hlink_lex STATIC
    tse/hlink/lex.hlink.c
)
target_include_directories(tse_hlink_lex PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/hlink
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/uri
)
target_compile_options(tse_hlink_lex PRIVATE -w)

# === Crawling module executable ===

set(CRAWL_SOURCES
    tse/Main.cpp
    tse/Crawl.cpp
    tse/Http.cpp
    tse/Page.cpp
    tse/Url.cpp
    tse/Md5.cpp
    tse/StrFun.cpp
    tse/Search.cpp
    tse/DataEngine.cpp
    tse/DatabaseEngine.cpp
    tse/FileEngine.cpp
    tse/TianwangFile.cpp
    tse/IsamFile.cpp
    tse/Link4SEFile.cpp
    tse/Res.cpp
)

add_executable(tse ${CRAWL_SOURCES})

target_include_directories(tse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tse
    ${CMAKE_CURRENT_SOURCE_DIR}/tse/include
)

target_link_libraries(tse PRIVATE
    tse_stack
    tse_misc
    tse_uri_lex
    tse_hlink_lex
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(tse PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-deprecated-declarations
    -Wno-deprecated-non-prototype
)

# === Index module tools ===

# ChSeg (Chinese segmentation) library
add_library(tse_chseg STATIC
    index/ChSeg/HzSeg.cpp
    index/ChSeg/Dict.cpp
)
target_include_directories(tse_chseg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/index
    ${CMAKE_CURRENT_SOURCE_DIR}/index/ChSeg
)

# Index module shared sources (NOT the same as tse/ versions)
set(INDEX_COMMON
    index/Url.cpp
    index/Md5.cpp
    index/StrFun.cpp
    index/Document.cpp
)

# DocIndex - build document index from raw data
add_executable(tse-docindex
    index/DocIndex.cpp
    ${INDEX_COMMON}
)
target_include_directories(tse-docindex PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/index
)
target_compile_options(tse-docindex PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

# DocSegment - segment documents
add_executable(tse-docsegment
    index/DocSegment.cpp
    ${INDEX_COMMON}
)
target_include_directories(tse-docsegment PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/index
    ${CMAKE_CURRENT_SOURCE_DIR}/index/ChSeg
)
target_link_libraries(tse-docsegment PRIVATE tse_chseg)
target_compile_options(tse-docsegment PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

# CrtForwardIdx - create forward index
add_executable(tse-fidx
    index/CrtForwardIdx.cpp
)
target_include_directories(tse-fidx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/index
)
target_compile_options(tse-fidx PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

# CrtInvertedIdx - create inverted index
add_executable(tse-iidx
    index/CrtInvertedIdx.cpp
)
target_include_directories(tse-iidx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/index
)
target_compile_options(tse-iidx PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

# === TSE Search Server ===
add_executable(tse-serve
    serve/main.cpp
)
target_include_directories(tse-serve PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/serve
)
target_link_libraries(tse-serve PRIVATE
    tse_chseg
)
target_compile_options(tse-serve PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -std=c++17
)
