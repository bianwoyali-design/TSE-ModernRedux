# Repository Guidelines

## Project Structure & Module Organization

This repository is a C++23 SQLite FTS5 search engine. Public headers live in `include/tse/`, implementation files in `src/`, tests in `tests/`, static UI assets in `web/`, and the default crawl seed file in `tse/seed`. The old Tianwang, ISAM, flex, CGI, and text-index pipelines have been removed. `CMakeLists.txt` is the only supported build entry point.

## Build, Test, and Development Commands

Configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Use the CLI locally:

```bash
./build/tse init --db Data/tse.db
./build/tse crawl --db Data/tse.db --seeds tse/seed --max-pages 50 --workers 10
./build/tse search --db Data/tse.db "query text"
./build/tse serve --db Data/tse.db --web web --port 8888
```

Build and run with Docker:

```bash
docker build -t tse-modernization .
docker run --rm -p 8888:8888 -v "$PWD/Data:/app/Data" tse-modernization
```

## Coding Style & Naming Conventions

Use C++23, RAII, and Almost Always Auto for local variables when the initializer makes the type clear. Project headers use `.hpp`, sources use `.cpp`, and headers use `#pragma once`. Do not add `using namespace std` to headers. Prefer `std::string`, containers, smart pointers, prepared SQLite statements, and scoped resource wrappers over raw owning pointers, `malloc/free`, `FILE*`, or manual cleanup. Keep names descriptive and namespace all project code under `tse`.

## Testing Guidelines

Tests are CTest-driven through `tse_tests`. Add focused tests for URL normalization, HTML extraction, segmentation, SQLite schema behavior, FTS search, pagination, snapshots, and crawler behavior. Use temporary database files and avoid network-dependent tests unless explicitly isolated.

## Commit & Pull Request Guidelines

The history does not define a strict convention. Use short imperative commit messages, for example `Add SQLite FTS search backend`. Pull requests should include a concise summary, commands run, behavior changes, and screenshots when `web/` output changes.

## Security & Configuration Tips

Do not commit `Data/*.db`, SQLite WAL/SHM files, build outputs, clangd caches, or crawl output. Keep OpenSSL, SQLite, and zlib discovery in CMake rather than hard-coding local machine paths.
