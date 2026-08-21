#include <gtest/gtest.h>

#include "dstore/common/sha256.h"

TEST(Sha256Test, KnownVector) {
  EXPECT_EQ(dstore::Sha256::hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
