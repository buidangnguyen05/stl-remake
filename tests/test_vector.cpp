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
#include <utility>

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

// --- P2.5: copy constructor, move constructor, swap ---

static_assert(std::is_nothrow_move_constructible_v<stl::vector<Tracked>>);

TEST_F(VectorFixture, CopyConstructorCounts) {
  const stl::vector<Tracked> source{Tracked(0), Tracked(1), Tracked(2), Tracked(3), Tracked(4)};
  Snapshot expected = Tracked::snapshot();
  {
    const stl::vector<Tracked> copy(source);
    expected.copy_ctor += 5;
    EXPECT_EQ(Tracked::snapshot(), expected);

    ASSERT_EQ(copy.size(), 5U);
    EXPECT_NE(copy.data(), source.data());
    for (std::size_t i = 0; i < copy.size(); ++i) {
      EXPECT_EQ(copy.data()[i], source.data()[i]) << "index " << i;
    }
  }
  EXPECT_EQ(Tracked::live(), 5);
}

TEST_F(VectorFixture, CopyConstructorRollsBackAtEveryPoint) {
  // the source is built before arming, so it doesn't use up the countdown
  const stl::vector<Tracked> source{Tracked(0), Tracked(1), Tracked(2), Tracked(3), Tracked(4),
                                    Tracked(5), Tracked(6), Tracked(7), Tracked(8), Tracked(9)};
  const Tracked *const source_data = source.data();
  for (std::size_t k = 0; k < source.size(); ++k) {
    const Tracked::ThrowAfter guard(k);
    EXPECT_THROW((void)stl::vector<Tracked>(source), TrackedError) << "k = " << k;
    EXPECT_EQ(Tracked::live(), 10) << "k = " << k;

    // the source is unchanged
    ASSERT_EQ(source.size(), 10U) << "k = " << k;
    EXPECT_EQ(source.data(), source_data) << "k = " << k;
    for (std::size_t i = 0; i < source.size(); ++i) {
      EXPECT_EQ(source.data()[i].value(), static_cast<int>(i)) << "k = " << k << ", index " << i;
    }
  }
}

TEST_F(VectorFixture, MoveConstructorMovesNoElements) {
  stl::vector<Tracked> a{Tracked(0), Tracked(1), Tracked(2), Tracked(3), Tracked(4)};
  const Tracked *const old_data = a.data();
  const Snapshot before = Tracked::snapshot();

  const stl::vector<Tracked> b = std::move(a);
  EXPECT_EQ(Tracked::snapshot(), before);
  EXPECT_EQ(b.data(), old_data);
  EXPECT_EQ(b.size(), 5U);

  // the moved-from vector is empty, with no buffer left
  EXPECT_EQ(a.size(), 0U);
  EXPECT_EQ(a.capacity(), 0U);
  EXPECT_EQ(a.data(), nullptr);

  // ...and it works like any other vector
  stl::vector<Tracked> other{Tracked(7), Tracked(8)};
  a.swap(other);
  ASSERT_EQ(a.size(), 2U);
  EXPECT_EQ(a.data()[0].value(), 7);
  EXPECT_EQ(a.data()[1].value(), 8);
  EXPECT_EQ(other.size(), 0U);
  // a is destroyed at the end of the test; the fixture checks that nothing leaked
}

TEST_F(VectorFixture, MemberSwapExchangesBuffers) {
  stl::vector<Tracked> a{Tracked(1), Tracked(2), Tracked(3)};
  stl::vector<Tracked> b{Tracked(4)};
  static_assert(noexcept(a.swap(b)));
  const Tracked *const a_data = a.data();
  const Tracked *const b_data = b.data();
  const Snapshot before = Tracked::snapshot();

  a.swap(b);
  EXPECT_EQ(Tracked::snapshot(), before);
  EXPECT_EQ(a.data(), b_data);
  EXPECT_EQ(b.data(), a_data);
  EXPECT_EQ(a.size(), 1U);
  EXPECT_EQ(b.size(), 3U);
}

TEST_F(VectorFixture, NonMemberSwapExchangesBuffers) {
  stl::vector<Tracked> a{Tracked(1), Tracked(2), Tracked(3)};
  stl::vector<Tracked> b{Tracked(4)};
  const Tracked *const a_data = a.data();
  const Tracked *const b_data = b.data();
  const Snapshot before = Tracked::snapshot();

  {
    // the "two-step": ADL must pick stl::swap over std::swap
    using std::swap;
    swap(a, b);
  }
  EXPECT_EQ(Tracked::snapshot(), before);
  EXPECT_EQ(a.data(), b_data);
  EXPECT_EQ(b.data(), a_data);
  EXPECT_EQ(a.size(), 1U);
  EXPECT_EQ(b.size(), 3U);

  // called by its full name, it swaps them back
  stl::swap(a, b);
  EXPECT_EQ(Tracked::snapshot(), before);
  EXPECT_EQ(a.data(), a_data);
  EXPECT_EQ(b.data(), b_data);
}

// --- P2.6: copy and move assignment ---
//
// Copy assignment exists in two versions: strong_copy_assignment (copy-and-swap) and
// basic_copy_assignment (reuses the capacity; operator= calls this one). Both are tested here.

namespace {

using Vec = stl::vector<Tracked>;
using CopyAssign = Vec &(Vec::*)(const Vec &);

enum class Guarantee { strong, basic };

// the three fixed scenarios from P2.6 step 3
struct Scenario {
  const char *name;
  std::size_t a_size;
  std::size_t a_capacity;
  std::size_t b_size;
};
constexpr Scenario more{
    .name = "(a) enough capacity, more elements", .a_size = 5, .a_capacity = 5, .b_size = 3};
constexpr Scenario fewer{
    .name = "(b) enough capacity, fewer elements", .a_size = 2, .a_capacity = 5, .b_size = 4};
constexpr Scenario too_small{
    .name = "(c) too little capacity", .a_size = 2, .a_capacity = 2, .b_size = 4};

// counter changes since `before`
Snapshot added_since(const Snapshot &before) {
  Snapshot now = Tracked::snapshot();
  now.default_ctor -= before.default_ctor;
  now.value_ctor -= before.value_ctor;
  now.copy_ctor -= before.copy_ctor;
  now.move_ctor -= before.move_ctor;
  now.copy_assign -= before.copy_assign;
  now.move_assign -= before.move_assign;
  now.destructor -= before.destructor;
  return now;
}

// a vector holding first, first + 1, ... with the given size and capacity
Vec make_vector(std::size_t size, std::size_t capacity, int first) {
  Vec v(capacity);
  for (std::size_t i = 0; i < capacity; ++i) {
    v.data()[i] = Tracked(first + static_cast<int>(i));
  }
  if (size < capacity) {
    // no reserve() yet: shrink with the reuse version, which keeps the buffer
    Vec head(size);
    for (std::size_t i = 0; i < size; ++i) {
      head.data()[i] = Tracked(first + static_cast<int>(i));
    }
    v.basic_copy_assignment(head);
  }
  return v;
}

bool holds(const Vec &v, std::size_t size, int first) {
  if (v.size() != size) {
    return false;
  }
  for (std::size_t i = 0; i < size; ++i) {
    if (v.data()[i].value() != first + static_cast<int>(i)) {
      return false;
    }
  }
  return true;
}

// runs `a = b` for one scenario and returns the counter changes it caused
Snapshot count_assignment(CopyAssign assign, const Scenario &s) {
  SCOPED_TRACE(s.name);
  Vec a = make_vector(s.a_size, s.a_capacity, 100);
  const Vec b = make_vector(s.b_size, s.b_size, 200);
  EXPECT_EQ(a.capacity(), s.a_capacity) << "scenario setup is wrong";

  const Snapshot before = Tracked::snapshot();
  (a.*assign)(b);
  const Snapshot added = added_since(before);

  EXPECT_TRUE(holds(a, s.b_size, 200)) << "a doesn't hold a copy of b";
  return added;
}

// runs the "every k" loop for one scenario, and checks `a` after each throw
void check_guarantee(CopyAssign assign, const Scenario &s, Guarantee expected) {
  SCOPED_TRACE(s.name);
  bool saw_change = false;
  std::size_t throws = 0;
  for (std::size_t k = 0;; ++k) {
    Vec a = make_vector(s.a_size, s.a_capacity, 100);
    const Vec b = make_vector(s.b_size, s.b_size, 200);
    const Tracked *const old_data = a.data();

    bool threw = false;
    {
      const Tracked::ThrowAfter guard(k);
      try {
        (a.*assign)(b);
      } catch (const TrackedError &) {
        threw = true;
      }
    }
    if (!threw) {
      break; // k is past the last operation that can throw
    }
    ++throws;

    // valid: every element is alive, and nothing leaked
    ASSERT_LE(a.size(), a.capacity()) << "k = " << k;
    for (std::size_t i = 0; i < a.size(); ++i) {
      (void)a.data()[i].value(); // fails if the element is dead
    }
    EXPECT_EQ(Tracked::live(), static_cast<std::ptrdiff_t>(a.size() + b.size())) << "k = " << k;

    const bool unchanged = a.data() == old_data && holds(a, s.a_size, 100);
    if (expected == Guarantee::strong) {
      EXPECT_TRUE(unchanged) << "k = " << k << ": a changed, so this isn't the strong guarantee";
    }
    saw_change = saw_change || !unchanged;
  }

  EXPECT_GT(throws, 0U) << "no operation ever threw";
  if (expected == Guarantee::basic) {
    EXPECT_TRUE(saw_change) << "a was never changed by a throw: this is stronger than basic";
  }
}

} // namespace

// exact counts caused by `a = b` alone
TEST_F(VectorFixture, StrongCopyAssignmentCounts) {
  const CopyAssign assign = &Vec::strong_copy_assignment;
  EXPECT_EQ(count_assignment(assign, more),
            (Snapshot{.copy_ctor = 3, .destructor = 5})); // (a) a: 5 of 5, b: 3
  EXPECT_EQ(count_assignment(assign, fewer),
            (Snapshot{.copy_ctor = 4, .destructor = 2})); // (b) a: 2 of 5, b: 4
  EXPECT_EQ(count_assignment(assign, too_small),
            (Snapshot{.copy_ctor = 4, .destructor = 2})); // (c) a: 2 of 2, b: 4
}

TEST_F(VectorFixture, BasicCopyAssignmentCounts) {
  const CopyAssign assign = &Vec::basic_copy_assignment;
  EXPECT_EQ(count_assignment(assign, more),
            (Snapshot{.copy_assign = 3, .destructor = 2})); // (a) a: 5 of 5, b: 3
  EXPECT_EQ(count_assignment(assign, fewer),
            (Snapshot{.copy_ctor = 2, .copy_assign = 2})); // (b) a: 2 of 5, b: 4
  EXPECT_EQ(count_assignment(assign, too_small),
            (Snapshot{.copy_ctor = 4, .destructor = 2})); // (c) a: 2 of 2, b: 4
}

// the guarantee of each version, checked after every possible throw
TEST_F(VectorFixture, StrongCopyAssignmentGuarantee) {
  const CopyAssign assign = &Vec::strong_copy_assignment;
  check_guarantee(assign, more, Guarantee::strong);
  check_guarantee(assign, fewer, Guarantee::strong);
  check_guarantee(assign, too_small, Guarantee::strong);
}

TEST_F(VectorFixture, BasicCopyAssignmentGuarantee) {
  const CopyAssign assign = &Vec::basic_copy_assignment;
  check_guarantee(assign, more, Guarantee::basic);
  check_guarantee(assign, fewer, Guarantee::basic);
  check_guarantee(assign, too_small, Guarantee::strong);
}

// `a = a` is safe, and a keeps its elements
TEST_F(VectorFixture, CopyAssignmentToSelf) {
  for (const CopyAssign assign : {&Vec::strong_copy_assignment, &Vec::basic_copy_assignment}) {
    Vec a = make_vector(3, 3, 100);
    const Snapshot before = Tracked::snapshot();
    (a.*assign)(a);
    EXPECT_EQ(added_since(before), Snapshot{});
    EXPECT_TRUE(holds(a, 3, 100));
  }
}

// operator=, self-assignment and move assignment

static_assert(std::is_nothrow_move_assignable_v<stl::vector<Tracked>>);

TEST_F(VectorFixture, CopyAssignmentOperatorToSelf) {
  Vec a = make_vector(3, 3, 100);
  const Tracked *const old_data = a.data();
  const auto &r = a; // written directly, `a = a` trips clang's -Wself-assign-overloaded
  a = r;
  EXPECT_EQ(a.data(), old_data);
  EXPECT_TRUE(holds(a, 3, 100));
}

TEST_F(VectorFixture, MoveAssignmentToSelf) {
  Vec a = make_vector(3, 3, 100);
  const Tracked *const old_data = a.data();
  const Snapshot before = Tracked::snapshot();
  auto &r = a; // written directly, `a = std::move(a)` trips -Werror=self-move
  a = std::move(r);
  EXPECT_EQ(added_since(before), Snapshot{});
  EXPECT_EQ(a.data(), old_data);
  EXPECT_TRUE(holds(a, 3, 100));
}

TEST_F(VectorFixture, MoveAssignmentCounts) {
  Vec a = make_vector(3, 3, 100); // the source
  Vec b = make_vector(2, 2, 200); // the target, not empty
  const Tracked *const a_data = a.data();
  const Snapshot before = Tracked::snapshot();

  b = std::move(a);
  // no element is built: only b's 2 old elements are destroyed
  EXPECT_EQ(added_since(before), (Snapshot{.destructor = 2}));
  EXPECT_EQ(b.data(), a_data);
  EXPECT_TRUE(holds(b, 3, 100));

  // the source is left empty and reusable
  EXPECT_EQ(a.size(), 0U);
  EXPECT_EQ(a.capacity(), 0U);
  a = b;
  EXPECT_TRUE(holds(a, 3, 100));
}

// --- P2.7: element access and capacity queries ---

namespace {
using IntVec = stl::vector<int>;
// const and non-const accessors return const and non-const results
static_assert(std::is_same_v<decltype(std::declval<IntVec &>()[0]), int &>);
static_assert(std::is_same_v<decltype(std::declval<const IntVec &>()[0]), const int &>);
static_assert(std::is_same_v<decltype(std::declval<IntVec &>().at(0)), int &>);
static_assert(std::is_same_v<decltype(std::declval<const IntVec &>().at(0)), const int &>);
static_assert(std::is_same_v<decltype(std::declval<IntVec &>().front()), int &>);
static_assert(std::is_same_v<decltype(std::declval<const IntVec &>().front()), const int &>);
static_assert(std::is_same_v<decltype(std::declval<IntVec &>().back()), int &>);
static_assert(std::is_same_v<decltype(std::declval<const IntVec &>().back()), const int &>);
static_assert(std::is_same_v<decltype(std::declval<IntVec &>().data()), int *>);
static_assert(std::is_same_v<decltype(std::declval<const IntVec &>().data()), const int *>);
} // namespace

TEST_F(VectorFixture, ElementAccessReadsAndWrites) {
  IntVec v{1, 2, 3};
  const IntVec &cv = v;

  // called with v and with cv, so it reads through both sets of overloads
  const auto read_all = [](auto &w) {
    EXPECT_EQ(w[1], 2);
    EXPECT_EQ(w.at(1), 2);
    EXPECT_EQ(w.front(), 1);
    EXPECT_EQ(w.back(), 3);
    EXPECT_EQ(*w.data(), 1);
  };
  read_all(v);
  read_all(cv);

  v[1] = 7;
  v.at(2) = 8;
  EXPECT_EQ(v[1], 7);
  EXPECT_EQ(v.at(2), 8);
  EXPECT_EQ(cv[1], 7);
  EXPECT_EQ(cv.back(), 8);
}

TEST_F(VectorFixture, AtThrowsOutOfRange) {
  IntVec v{1, 2, 3};
  const IntVec &cv = v;
  EXPECT_THROW((void)v.at(v.size()), std::out_of_range);
  EXPECT_THROW((void)cv.at(cv.size()), std::out_of_range);
  EXPECT_NO_THROW((void)v.at(v.size() - 1));
}

TEST_F(VectorFixture, CapacityQueries) {
  const IntVec empty_vec;
  EXPECT_TRUE(empty_vec.empty());
  EXPECT_EQ(empty_vec.size(), 0U);

  const IntVec v{1, 2, 3};
  EXPECT_FALSE(v.empty());
  EXPECT_EQ(v.size(), 3U);
  EXPECT_EQ(v.capacity(), 3U);
}
