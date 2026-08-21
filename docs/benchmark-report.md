# Benchmark Report

The benchmark executables are `chunk_bench` and `cluster_bench`.

`chunk_bench` measures fixed-size object chunking throughput over a 128 MiB synthetic object.

`cluster_bench` measures Raft append throughput and average append latency.

`recovery_bench` measures WAL replay recovery time over 100,000 records.

Run:

```bash
cmake -S . -B build -DDSTORE_BUILD_TESTS=OFF -DDSTORE_BUILD_BENCHMARKS=ON
cmake --build build --target chunk_bench cluster_bench recovery_bench
./build/chunk_bench
./build/cluster_bench
./build/recovery_bench
```

Reported fields:

- `chunked_bytes`: total bytes processed
- `chunks`: number of generated chunks
- `elapsed_ms`: wall-clock chunking time
- `throughput_mib_per_sec`: chunking throughput
- `raft_append_avg_ns`: average Raft log append latency
- `recovery_elapsed_ms`: WAL replay recovery time

The current workspace shell does not expose a usable C++20 compiler, so local benchmark numbers were not produced on this machine. Run `scripts/run_benchmarks.ps1` on a C++20 toolchain to regenerate `docs/performance-results.md`.
