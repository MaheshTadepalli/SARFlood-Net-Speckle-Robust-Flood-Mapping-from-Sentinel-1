FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake libsqlite3-dev ca-certificates unzip && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDSTORE_BUILD_TESTS=OFF && cmake --build build -j

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libsqlite3-0 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/dstore-server /app/dstore-server
COPY --from=build /src/build/dstore-cli /app/dstore-cli
COPY config /app/config
EXPOSE 8080
CMD ["/app/dstore-server", "--config", "/app/config/dev.toml"]
