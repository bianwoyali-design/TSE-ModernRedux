FROM debian:bookworm-slim AS build

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        libsqlite3-dev \
        libssl-dev \
        pkg-config \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt .
COPY include include
COPY src src
COPY tests tests

RUN cmake -S . -B /build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build /build --parallel
RUN ctest --test-dir /build --output-on-failure

FROM debian:bookworm-slim AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libsqlite3-0 \
        libssl3 \
        zlib1g \
    && useradd --uid 1000 --create-home --home-dir /app --shell /usr/sbin/nologin tse \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /build/tse /usr/local/bin/tse
COPY web web
COPY tse/seed tse/seed

RUN mkdir -p /app/Data && chown -R tse:tse /app

USER tse

VOLUME ["/app/Data"]
EXPOSE 8888

ENTRYPOINT ["tse"]
CMD ["serve", "--db", "/app/Data/tse.db", "--web", "/app/web", "--port", "8888"]
