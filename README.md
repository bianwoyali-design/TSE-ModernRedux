# TSE — Tiny Search Engine

An educational web search engine originally created at Peking University's Network Lab (2003–2004), modernized for macOS.

TSE implements a complete search engine pipeline: **crawl → index → serve**.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.logicalcpu)

# Start the web server
./tse-serve --web ../web --port 8888
```

Open **http://localhost:8888/** in your browser.

## How It Works

### 1. Crawling — `./tse -c seeds.txt`

The crawler downloads web pages, extracts hyperlinks, and saves them in Tianwang format.

- Multi-threaded (10 workers by default)
- HTTP/1.0 with keep-alive
- MD5-based URL and page deduplication
- IP block filtering

```
./tse -c seeds.txt
# Output: Tianwang.raw.<tid>, Link4SE.raw.<tid>, visited.all, ...
```

### 2. Indexing — pipeline tools

Build a searchable index from crawled pages:

```bash
# Step 1: Create document index
./tse-docindex
# Output: Data/Doc.idx, Data/Url.idx

# Step 2: Sort and deduplicate URLs
sort Data/Url.idx | uniq > Data/Url.idx.sort_uniq

# Step 3: Segment documents (Chinese word segmentation)
./tse-docsegment Tianwang.raw.*
# Output: Tianwang.raw.*.seg

# Step 4: Create forward index (term → docID)
LANG=en ./tse-fidx Tianwang.raw.*.seg > moon.fidx

# Step 5: Sort forward index
sort moon.fidx > moon.fidx.sort

# Step 6: Create inverted index (term → docID list)
./tse-iidx moon.fidx.sort > Data/sun.iidx
```

### 3. Serving — `./tse-serve`

An embedded HTTP server loads the index and provides a modern web search interface.

```
./tse-serve --data ./Data --web ../web --port 8888
```

## Project Structure

```
TSE/
├── CMakeLists.txt          # Build system
├── README.md
├── tse/                    # Crawling module (modernized)
│   ├── Main.cpp            # Entry point
│   ├── Crawl.cpp/h         # Crawler orchestrator
│   ├── Http.cpp/h          # HTTP/1.0 client
│   ├── Page.cpp/h          # HTML parsing
│   ├── Url.cpp/h           # URL parsing & DNS
│   ├── Md5.cpp/h           # MD5 hashing
│   ├── hlink/              # HTML link lexer (flex)
│   ├── uri/                # URI parser (flex)
│   ├── stack/              # Generic typed stack (C)
│   └── lib/                # Utilities
├── index/                  # Indexing module (modernized)
│   ├── DocIndex.cpp        # Document index builder
│   ├── DocSegment.cpp      # Document segmenter
│   ├── CrtForwardIdx.cpp   # Forward index
│   ├── CrtInvertedIdx.cpp  # Inverted index
│   └── ChSeg/              # Chinese word segmentation
├── serve/                  # Web server (NEW)
│   ├── main.cpp            # Server entry point
│   ├── SimpleServer.hpp    # Embedded HTTP server
│   └── SearchEngine.hpp    # Query engine
├── web/                    # Web UI (NEW)
│   ├── index.html          # Search interface
│   ├── style.css           # Modern styling
│   └── app.js              # Client-side search
└── vendor/                 # Third-party code
```

## Modernizations

- **macOS Apple Silicon** — compiles and runs natively
- **C++17** — modern standard, cleaner code
- **CMake** — cross-platform build system
- **Embedded HTTP server** — replaces 2003 CGI model
- **Modern web UI** — HTML5, CSS3, AJAX search
- **UTF-8** — full UTF-8 support throughout

## Requirements

- macOS 14+ with Xcode Command Line Tools
- CMake 3.20+
- flex (included with macOS)

## Original Project

TSE was created by **YAN Hongfei** (闫宏飞) at the Network Lab of Peking University.

- Web: http://net.pku.edu.cn/~webg/src/TSE/
- Contact (historical): yanhf@pku.edu.cn

The original source tarballs are preserved in this directory for reference.

## License

Educational use. See original source files for any additional license information.
