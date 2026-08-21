#include <gtest/gtest.h>

#include "dstore/common/lru_cache.h"

TEST(LruCacheTest, EvictsLeastRecentlyUsed) {
  dstore::LruCache<int, int> cache(2);
  cache.put(1, 10);
  cache.put(2, 20);
  EXPECT_EQ(cache.get(1), 10);
  cache.put(3, 30);
  EXPECT_FALSE(cache.get(2).has_value());
  EXPECT_EQ(cache.get(1), 10);
  EXPECT_EQ(cache.get(3), 30);
}
