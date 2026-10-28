#include "stl/vector.hpp"
#include "support/tracked.hpp"
#include "support/tracked_fixture.hpp"

#include <gtest/gtest.h>

namespace {

class VectorFixture : public TrackedFixture {};

struct alignas(64) Big {
  char c;
};

} // namespace

TEST_F(VectorFixture, DefaultConstructor) {
  {
    stl::vector<int> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
    EXPECT_EQ(v.data(), nullptr);
  }
  {
    stl::vector<Big> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
    EXPECT_EQ(v.data(), nullptr);
  }
  {
    stl::vector<stl::test::Tracked> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
    EXPECT_EQ(v.data(), nullptr);
  }
  EXPECT_EQ(stl::test::Tracked::snapshot(), stl::test::Tracked::Snapshot());
}
