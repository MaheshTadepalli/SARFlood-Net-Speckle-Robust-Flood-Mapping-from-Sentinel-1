# Deployment Guide

## Local

```bash
cmake -S . -B build -DDSTORE_BUILD_TESTS=ON
cmake --build build --config Release
./build/dstore-server --config config/dev.toml
```

## Docker Compose

```bash
docker compose -f docker/docker-compose.yml up --build
```

The default Compose topology starts three storage nodes and Prometheus:

- object API: `localhost:8080`
- Prometheus: `localhost:9090`

The default token is `admin-token`. Override it in the node config files for non-local deployments.

## Operational Notes

- Put metadata on durable storage.
- Put chunks on a high-capacity volume.
- Keep WAL mode enabled for SQLite.
- Set `replication_factor`, `read_quorum`, and `write_quorum` according to the number of healthy storage nodes.
- Scrape `/metrics` for operational dashboards.
