# TSE - Tiny Search Engine

TSE is a compact C++23 search engine for crawling pages, storing documents in SQLite, and serving ranked full-text search results through a small embedded HTTP server.

The project no longer uses the legacy Tianwang raw files, ISAM files, flex parsers, CGI tools, or text-based inverted indexes. SQLite FTS5 is the only index and storage backend.

## Quick Start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

./build/tse init --db Data/tse.db
./build/tse crawl --db Data/tse.db --seeds tse/seed --max-pages 50 --workers 10
./build/tse serve --db Data/tse.db --web web --port 8888
```

Open `http://localhost:8888/` in a browser.

## Docker

Build the image:

```bash
docker build -t tse-modernization .
```

Use a host-mounted `Data/` directory so the SQLite database survives container
restarts:

```bash
mkdir -p Data
docker run --rm -v "$PWD/Data:/app/Data" tse-modernization init --db /app/Data/tse.db
docker run --rm -v "$PWD/Data:/app/Data" tse-modernization crawl --db /app/Data/tse.db --seeds /app/tse/seed --max-pages 50 --workers 4
docker run --rm -p 8888:8888 -v "$PWD/Data:/app/Data" tse-modernization
```

Open `http://localhost:8888/` after the server starts.

## Apple Container

Apple's `container` CLI can build and run the same `Dockerfile` on Apple
silicon Macs with macOS 26 or later. Install `container`, then start its system
service:

```bash
container system start
```

Build the image:

```bash
container build -t tse-modernization .
```

Use a named volume for the SQLite database:

```bash
container volume create tse-data
container run --rm -v tse-data:/app/Data tse-modernization init --db /app/Data/tse.db
container run --rm -v tse-data:/app/Data tse-modernization crawl --db /app/Data/tse.db --seeds /app/tse/seed --max-pages 50 --workers 4
container run --name tse-web --rm -d -p 8888:8888 -v tse-data:/app/Data tse-modernization
```

Open `http://localhost:8888/`. Stop the server with:

```bash
container stop tse-web
```

With Docker Compose:

```bash
mkdir -p Data
docker compose build
docker compose run --rm tse init --db /app/Data/tse.db
docker compose run --rm tse crawl --db /app/Data/tse.db --seeds /app/tse/seed --max-pages 50 --workers 4
docker compose up
```

## CLI

```bash
./build/tse init --db Data/tse.db
./build/tse crawl --db Data/tse.db --seeds tse/seed --max-pages 1000 --workers 10 --max-depth 2
./build/tse search --db Data/tse.db "query text" --page 1 --limit 20
./build/tse serve --db Data/tse.db --web web --port 8888
./build/tse stats --db Data/tse.db
./build/tse vacuum --db Data/tse.db
```

Use `--workers`, `--max-pages`, and `--max-depth` to control crawl concurrency and scope.

## Architecture

```
include/tse/        Public C++23 headers
src/                Core implementation and CLI
tests/              CTest-based unit/integration tests
tse/seed            Default crawl seed file
web/                Static search UI
CMakeLists.txt      Build definition
```

Core components:

- `Database` initializes SQLite, manages schema, stores documents/links/queue state, and queries FTS5 with BM25 ranking.
- `Crawler` fetches HTTP/HTTPS pages, extracts text and links, and writes normalized documents into SQLite.
- `Segmenter` adds ASCII tokens plus UTF-8 character and bigram tokens for broader Chinese search.
- `SimpleServer` serves `web/` and the `/api/search` and `/api/snapshot` endpoints.

## API

- `GET /api/search?q=...&p=1` returns JSON with query, total, elapsed time, segmented terms, and ranked result objects.
- `GET /api/snapshot?url=...&word=...` returns cached plain-text page content with highlighted query terms.

## Development

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Requirements:

- macOS with Xcode Command Line Tools
- CMake 3.20+
- SQLite3 with FTS5
- OpenSSL
- zlib

## Notes

Generated databases and build output should not be committed. The default database path is `Data/tse.db`.
