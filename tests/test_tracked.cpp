//
// Created by buidangnguyen05 on 10/27/26.
//
#include <gtest/gtest.h>
#include <optional>
#include <support/tracked.hpp>
#include <support/tracked_fixture.hpp>
#include <utility>

using stl::test::Tracked;
using Snapshot = Tracked::Snapshot;

TEST_F(TrackedFixture, EachSpecialMemberBumpsOnlyItsCounter) {
  {
    Tracked::reset();
    const Snapshot before = Tracked::snapshot();
    Tracked t;
    Snapshot expected = before;
    ++expected.default_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(t.value(), 0);
  }
  {
    Tracked::reset();
    const Snapshot before = Tracked::snapshot();
    Tracked t(7);
    Snapshot expected = before;
    ++expected.value_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(t.value(), 7);
  }
  {
    Tracked::reset();
    Tracked a(1);
    const Snapshot before = Tracked::snapshot();
    Tracked b = a;
    Snapshot expected = before;
    ++expected.copy_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(b.value(), 1);
  }
  {
    Tracked::reset();
    Tracked a(1);
    const Snapshot before = Tracked::snapshot();
    Tracked b = std::move(a);
    Snapshot expected = before;
    ++expected.move_ctor;
    EXPECT_EQ(Tracked::snapshot(), expected);
    EXPECT_EQ(b.value(), 1);
  }
  {
    Tracked::reset();
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
    Tracked::reset();
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
    Tracked::reset();
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
    Tracked a(1);
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
  Tracked a(1);
  Tracked b(2);
  EXPECT_EQ(Tracked::live(), 2);
  {
    Tracked c = a;
    EXPECT_EQ(Tracked::live(), 3);
  }
  EXPECT_EQ(Tracked::live(), 2);
}

TEST_F(TrackedFixture, DISABLED_DeliberateLeak) {
  [[maybe_unused]] Tracked *leak = new Tracked(42);
  EXPECT_EQ(Tracked::live(), 1);
}
