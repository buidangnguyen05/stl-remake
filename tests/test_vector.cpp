#include "stl/vector.hpp"
#include "support/tracked.hpp"
#include "support/tracked_fixture.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <type_traits>

using stl::test::Tracked;
using stl::test::TrackedError;
using Snapshot = Tracked::Snapshot;

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

// --- P2.4: constructors ---

static_assert(!std::is_convertible_v<std::size_t, stl::vector<int>>,
              "vector(size_type) must be explicit");

TEST_F(VectorFixture, SizeConstructorValueInitializes) {
  const stl::vector<int> v(5);
  ASSERT_EQ(v.size(), 5U);
  EXPECT_EQ(v.capacity(), 5U);
  for (std::size_t i = 0; i < v.size(); ++i) {
    EXPECT_EQ(v.data()[i], 0) << "index " << i;
  }
}

TEST_F(VectorFixture, SizeConstructorCounts) {
  {
    const stl::vector<Tracked> v(5);
    EXPECT_EQ(v.size(), 5U);
    // write your prediction before running: which counters change, and by how much?
    EXPECT_EQ(Tracked::snapshot(), (Snapshot{.default_ctor = 5}));
  }
  EXPECT_EQ(Tracked::live(), 0);
}

TEST_F(VectorFixture, FillConstructorCounts) {
  const Tracked t(7);
  const Snapshot before = Tracked::snapshot();
  {
    const stl::vector<Tracked> v(5, t);
    ASSERT_EQ(v.size(), 5U);
    for (std::size_t i = 0; i < v.size(); ++i) {
      EXPECT_EQ(v.data()[i].value(), 7) << "index " << i;
    }
    // write your prediction before running: the counts *added* by the constructor
    Snapshot added = Tracked::snapshot();
    added.value_ctor -= before.value_ctor;
    EXPECT_EQ(added, (Snapshot{.copy_ctor = 5}));
  }
  EXPECT_EQ(Tracked::live(), 1);
}

TEST_F(VectorFixture, InitializerListConstructorCounts) {
  {
    const stl::vector<Tracked> v{Tracked(1), Tracked(2), Tracked(3)};
    ASSERT_EQ(v.size(), 3U);
    EXPECT_EQ(v.data()[0].value(), 1);
    EXPECT_EQ(v.data()[2].value(), 3);
    // write your prediction before running: count everything, including the list itself
    EXPECT_EQ(Tracked::snapshot(), (Snapshot{.value_ctor = 3, .copy_ctor = 3, .destructor = 3}));
  }
  EXPECT_EQ(Tracked::live(), 0);
}

TEST_F(VectorFixture, SizeConstructorRollsBackAtEveryPoint) {
  for (std::size_t k = 0; k < 10; ++k) {
    const Tracked::ThrowAfter guard(k);
    EXPECT_THROW((void)stl::vector<Tracked>(10), TrackedError) << "k = " << k;
    EXPECT_EQ(Tracked::live(), 0) << "k = " << k;
  }
}

TEST_F(VectorFixture, FillConstructorRollsBackAtEveryPoint) {
  const Tracked source(7); // built before arming, so it doesn't use up the countdown
  for (std::size_t k = 0; k < 10; ++k) {
    const Tracked::ThrowAfter guard(k);
    EXPECT_THROW((void)stl::vector<Tracked>(10, source), TrackedError) << "k = " << k;
    EXPECT_EQ(Tracked::live(), 1) << "k = " << k;
  }
}

TEST_F(VectorFixture, InitializerListConstructorRollsBackAtEveryPoint) {
  // the list's items are built before arming, and live as long as `list`
  const std::initializer_list<Tracked> list = {Tracked(0), Tracked(1), Tracked(2), Tracked(3),
                                               Tracked(4), Tracked(5), Tracked(6), Tracked(7),
                                               Tracked(8), Tracked(9)};
  const std::ptrdiff_t baseline = Tracked::live();
  for (std::size_t k = 0; k < list.size(); ++k) {
    const Tracked::ThrowAfter guard(k);
    EXPECT_THROW((void)stl::vector<Tracked>(list), TrackedError) << "k = " << k;
    EXPECT_EQ(Tracked::live(), baseline) << "k = " << k;
  }
}

TEST_F(VectorFixture, OverAlignedElements) {
  for (const std::size_t n : {1U, 2U, 3U, 7U, 64U}) {
    const stl::vector<Big> v(n);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(v.data()) % 64, 0U) << "n = " << n;
  }
}

TEST_F(VectorFixture, ImmovableElements) {
  // std::mutex can't be moved or copied; vector(n) must not need either
  const stl::vector<std::mutex> v(3);
  EXPECT_EQ(v.size(), 3U);
}

TEST_F(VectorFixture, TooLargeSizeThrowsLengthError) {
  const stl::vector<int> probe;
  EXPECT_THROW((void)stl::vector<int>(probe.max_size() + 1), std::length_error);
  // so large that n * sizeof(int) would wrap around without the check
  EXPECT_THROW((void)stl::vector<int>(std::numeric_limits<std::size_t>::max() / 2),
               std::length_error);
}
