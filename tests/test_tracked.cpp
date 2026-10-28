//
// Created by buidangnguyen05 on 10/27/26.
//
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <memory>
#include <new>
#include <optional>
#include <support/tracked.hpp>
#include <support/tracked_fixture.hpp>
#include <utility>
#include <vector>

using stl::test::Tracked;
using Snapshot = Tracked::Snapshot;

TEST_F(TrackedFixture, EachSpecialMemberBumpsOnlyItsCounter) {
  {
    const Snapshot before = Tracked::snapshot();
    Tracked t;
    Snapshot expected = before;
    ++expected.default_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(t.value(), 0);
  }
  {
    const Snapshot before = Tracked::snapshot();
    Tracked t(7);
    Snapshot expected = before;
    ++expected.value_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(t.value(), 7);
  }
  {
    Tracked a(1);
    const Snapshot before = Tracked::snapshot();
    Tracked b = a;
    Snapshot expected = before;
    ++expected.copy_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(b.value(), 1);
  }
  {
    Tracked a(1);
    const Snapshot before = Tracked::snapshot();
    Tracked b = std::move(a);
    Snapshot expected = before;
    ++expected.move_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(b.value(), 1);
  }
  {
    Tracked a(1);
    Tracked b(2);
    const Snapshot before = Tracked::snapshot();
    b = a;
    Snapshot expected = before;
    ++expected.copy_assign;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(b.value(), 1);
  }
  {
    Tracked a(1);
    Tracked b(2);
    const Snapshot before = Tracked::snapshot();
    b = std::move(a);
    Snapshot expected = before;
    ++expected.move_assign;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(b.value(), 1);
  }
  {
    std::optional<Tracked> t(std::in_place, 1);
    const Snapshot before = Tracked::snapshot();
    t.reset(); // runs exactly the destructor
    Snapshot expected = before;
    ++expected.destructor;
    EXPECT_EQ(Tracked::snapshot(), expected);
  }
}

TEST_F(TrackedFixture, DefaultCopyMoveSequence) {
  {
    Tracked a;
    Tracked b = a;
    Tracked c = std::move(a);
    EXPECT_EQ(Tracked::snapshot(), (Snapshot{.default_ctor = 1, .copy_ctor = 1, .move_ctor = 1}));
    EXPECT_EQ(Tracked::live(), 3);
  }
  EXPECT_EQ(Tracked::snapshot(),
            (Snapshot{.default_ctor = 1, .copy_ctor = 1, .move_ctor = 1, .destructor = 3}));
}

TEST_F(TrackedFixture, ResetAndSnapshotComparison) {
  {
    const Tracked a(1);
    const Snapshot s1 = Tracked::snapshot();
    const Snapshot s2 = Tracked::snapshot();
    EXPECT_EQ(s1, s2); // nothing in between

    Tracked b = a;
    const Snapshot s3 = Tracked::snapshot();
    EXPECT_NE(s2, s3); // one copy in between
  }

  Tracked::reset();
  const Snapshot r1 = Tracked::snapshot();
  Tracked::reset();
  const Snapshot r2 = Tracked::snapshot();
  EXPECT_EQ(r1, r2);
  EXPECT_EQ(r1, Snapshot{});
}

TEST_F(TrackedFixture, Normal) {
  const Tracked a(1);
  const Tracked b(2);
  EXPECT_EQ(Tracked::live(), 2);
  {
    Tracked c = a;
    EXPECT_EQ(Tracked::live(), 3);
  }
  EXPECT_EQ(Tracked::live(), 2);
}

namespace {
class TrackedMemberFixture : public TrackedFixture {
protected:
  std::vector<Tracked> m_items;
};
} // namespace

TEST_F(TrackedMemberFixture, MembersOfDerivedFixtureAreNotLeaks) {
  m_items.emplace_back(1);
  m_items.emplace_back(2);
  EXPECT_EQ(Tracked::live(), 2);
}

TEST_F(TrackedFixture, MovedFromHoldsMovedFromValue) {
  Tracked a(5);
  const Tracked b = std::move(a);
  EXPECT_EQ(a.value(), Tracked::moved_from); // NOLINT(bugprone-use-after-move): on purpose
  EXPECT_EQ(b.value(), 5);

  Tracked c(6);
  Tracked d(7);
  d = std::move(c);
  EXPECT_EQ(c.value(), Tracked::moved_from); // NOLINT(bugprone-use-after-move): on purpose
  EXPECT_EQ(d.value(), 6);
}

namespace {
// raw storage for one Tracked, the way a vector holds its elements
struct RawSlot {
  alignas(Tracked) unsigned char bytes[sizeof(Tracked)];

  Tracked *construct(int value) { return ::new (static_cast<void *>(bytes)) Tracked(value); }
};
} // namespace

TEST_F(TrackedFixture, ReconstructInSameStorageIsFine) {
  RawSlot slot;
  Tracked *p = slot.construct(1);
  std::destroy_at(p);
  p = slot.construct(2);
  EXPECT_EQ(p->value(), 2);
  std::destroy_at(p);
}

TEST_F(TrackedFixture, DoubleDestructionIsReported) {
  RawSlot slot;
  Tracked *p = slot.construct(1);
  std::destroy_at(p);
  EXPECT_NONFATAL_FAILURE(std::destroy_at(p), "destroyed twice");
}

TEST_F(TrackedFixture, UseAfterDestructionIsReported) {
  RawSlot slot;
  Tracked *p = slot.construct(1);
  std::destroy_at(p);
  EXPECT_NONFATAL_FAILURE(static_cast<void>(p->value()), "read after it was destroyed");
}

TEST_F(TrackedFixture, ConstructOverLiveObjectIsReported) {
  RawSlot slot;
  slot.construct(1);
  Tracked *p = nullptr;
  EXPECT_NONFATAL_FAILURE(p = slot.construct(2), "still alive");
  std::destroy_at(p);
  Tracked::reset(); // two constructions, one destruction: not a leak this test cares about
}

TEST_F(TrackedFixture, ThrowAfterThreeCopies) {
  const Tracked source(1);
  const Tracked::ThrowAfter guard(3);
  EXPECT_NO_THROW(Tracked{source});
  EXPECT_NO_THROW(Tracked{source});
  EXPECT_NO_THROW(Tracked{source});

  const Snapshot before = Tracked::snapshot();
  try {
    const Tracked fourth(source);
    ADD_FAILURE() << "the 4th copy did not throw";
  } catch (const stl::test::TrackedError &) {
    EXPECT_EQ(Tracked::snapshot(), before); // the throw came before any state change
  }
  EXPECT_EQ(Tracked::live(), 1);
}

TEST_F(TrackedFixture, ThrowAfterCountsConstructions) {
  const Tracked::ThrowAfter guard(2);
  EXPECT_NO_THROW(Tracked{});
  EXPECT_NO_THROW(Tracked{1});
  EXPECT_THROW(Tracked{2}, stl::test::TrackedError);
  EXPECT_EQ(Tracked::live(), 0);
}

TEST_F(TrackedFixture, ThrowAfterCountsThrowingMoves) {
  using stl::test::ThrowingMoveTracked;
  ThrowingMoveTracked a(1);
  ThrowingMoveTracked b(2);
  const Tracked::ThrowAfter guard(1);
  ThrowingMoveTracked c = std::move(a);
  EXPECT_THROW(c = std::move(b), stl::test::TrackedError);
  EXPECT_EQ(b.value(), 2); // the throw came before b was moved from
}

TEST_F(TrackedFixture, NoexceptMovesNeverThrow) {
  Tracked a(1);
  const Tracked::ThrowAfter guard(0); // the very next operation that may throw, throws
  EXPECT_NO_THROW(Tracked{std::move(a)});
  EXPECT_THROW(Tracked{a}, stl::test::TrackedError);
}

TEST_F(TrackedFixture, ThrowAfterDisarmsAtScopeExit) {
  {
    const Tracked::ThrowAfter guard(0);
  }
  EXPECT_NO_THROW(Tracked{});
}

TEST_F(TrackedFixture, ResetDisarms) {
  const Tracked::ThrowAfter guard(0);
  Tracked::reset();
  EXPECT_NO_THROW(Tracked{});
}

TEST_F(TrackedFixture, DISABLED_DeliberateLeak) {
  [[maybe_unused]] auto *leak = new Tracked(42);
  EXPECT_EQ(Tracked::live(), 1);
}
