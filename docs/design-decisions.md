# Design Decisions

## SQLite First

SQLite is used for the first metadata backend because it gives transactional semantics, indexes, WAL durability, and an easy local deployment path. The `MetadataStore` boundary is narrow enough to replace with RocksDB or a replicated metadata service later.

## Content-Addressed Chunks

Chunks are named by SHA-256. This makes deduplication natural, turns integrity verification into a constant operation at every read, and keeps disk layout simple.

## Custom Wire Format

Public traffic uses REST because it is easy to operate and test. Internal service-to-service messages use a compact binary frame with a message type, correlation ID, and payload, avoiding a hard dependency on Protocol Buffers while keeping the protocol explicit.

## Quorum Planning

Read and write plans are computed from healthy nodes through consistent hashing and then executed through the peer chunk API. This makes failover and data migration deterministic: once membership changes, desired chunk placement is recomputed from the same key.

## WAL And Replay

Object mutations are logged before metadata commit. WAL records include checksums so crash replay can reject torn or corrupt records. Snapshot manifests capture a recovery point and allow WAL compaction.

At startup the server replays and validates WAL records before accepting traffic. The recovery model is intentionally simple: durable mutation records are the source of truth for detecting incomplete work after a crash.

## Raft Scope

Raft is implemented as a reusable state machine with voting, append-entry semantics, follower replication indexes, and majority commit advancement. It is deliberately separated from transport so consensus mechanics remain easy to test and discuss.

## Logging

The code uses spdlog when available and keeps a structured stderr fallback so the project remains buildable in minimal containers.
