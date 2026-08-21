#include <gtest/gtest.h>

#include "dstore/network/wire.h"

TEST(WireTest, RoundTripsFrame) {
  dstore::WireFrame frame{7, "request-1", {'a', 'b', 'c'}};
  auto encoded = dstore::encode_frame(frame);
  auto decoded = dstore::decode_frame(encoded);
  ASSERT_TRUE(decoded.ok());
  EXPECT_EQ(decoded.value().type, frame.type);
  EXPECT_EQ(decoded.value().correlation_id, frame.correlation_id);
  EXPECT_EQ(decoded.value().payload, frame.payload);
}
