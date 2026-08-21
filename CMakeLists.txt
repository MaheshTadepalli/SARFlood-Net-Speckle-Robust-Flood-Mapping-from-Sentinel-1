cmake_minimum_required(VERSION 3.22)
project(distributed_object_store VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DSTORE_BUILD_TESTS "Build tests" ON)
option(DSTORE_BUILD_BENCHMARKS "Build benchmarks" ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(spdlog QUIET)

add_library(dstore
  src/common/auth.cpp
  src/common/config.cpp
  src/common/logger.cpp
  src/common/metrics.cpp
  src/common/sha256.cpp
  src/common/thread_pool.cpp
  src/cluster/background_worker.cpp
  src/cluster/distributed_chunk_service.cpp
  src/cluster/hinted_handoff.cpp
  src/cluster/node_client.cpp
  src/cluster/raft.cpp
  src/cluster/repair_worker.cpp
  src/durability/wal.cpp
  src/metadata/metadata_store.cpp
  src/network/http_server.cpp
  src/network/wire.cpp
  src/storage/chunk_store.cpp
  src/storage/consistent_hash.cpp
  src/storage/replication.cpp
)

target_include_directories(dstore PUBLIC include)
target_link_libraries(dstore PUBLIC Threads::Threads SQLite::SQLite3)
if(spdlog_FOUND)
  target_link_libraries(dstore PUBLIC spdlog::spdlog)
  target_compile_definitions(dstore PUBLIC DSTORE_USE_SPDLOG=1)
endif()

if(WIN32)
  target_link_libraries(dstore PUBLIC ws2_32)
endif()

add_executable(dstore-server server/main.cpp)
target_link_libraries(dstore-server PRIVATE dstore)

add_executable(dstore-cli client/main.cpp)
target_link_libraries(dstore-cli PRIVATE dstore)

if(DSTORE_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  add_executable(dstore-tests
    tests/sha256_test.cpp
    tests/lru_cache_test.cpp
    tests/consistent_hash_test.cpp
    tests/chunk_store_test.cpp
    tests/replication_test.cpp
    tests/wire_test.cpp
    tests/raft_test.cpp
    tests/wal_test.cpp
    tests/read_repair_test.cpp
    tests/hinted_handoff_test.cpp
  )
  target_link_libraries(dstore-tests PRIVATE dstore GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(dstore-tests)
endif()

if(DSTORE_BUILD_BENCHMARKS)
  add_executable(chunk_bench benchmarks/chunk_bench.cpp)
  target_link_libraries(chunk_bench PRIVATE dstore)
  add_executable(cluster_bench benchmarks/cluster_bench.cpp)
  target_link_libraries(cluster_bench PRIVATE dstore)
  add_executable(recovery_bench benchmarks/recovery_bench.cpp)
  target_link_libraries(recovery_bench PRIVATE dstore)
endif()
