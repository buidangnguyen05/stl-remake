#pragma once
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <gtest/gtest.h>
#include <new>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace stl::test {

namespace detail {

// Allocates with malloc, so the registry stays invisible to tests that count operator new calls.
template <class T> struct MallocAllocator {
  using value_type = T;

  MallocAllocator() = default;
  template <class U> explicit MallocAllocator(const MallocAllocator<U> &) noexcept {}

  [[nodiscard]] T *allocate(std::size_t n) {
    if (void *p = std::malloc(n * sizeof(T))) {
      return static_cast<T *>(p);
    }
    throw std::bad_alloc();
  }

  void deallocate(T *p, std::size_t) noexcept { std::free(p); }

  friend bool operator==(const MallocAllocator &, const MallocAllocator &) = default;
};

} // namespace detail

// the exception that an armed Tracked::ThrowAfter makes Tracked throw
class TrackedError : public std::runtime_error {
public:
  TrackedError() : std::runtime_error("Tracked: injected throw") {}
};

class Tracked {
public:
  struct Snapshot {
    std::size_t default_ctor = 0;
    std::size_t value_ctor = 0;
    std::size_t copy_ctor = 0;
    std::size_t move_ctor = 0;
    std::size_t copy_assign = 0;
    std::size_t move_assign = 0;
    std::size_t destructor = 0;

    auto operator<=>(const Snapshot &) const = default;

    // for readable failure messages
    friend std::ostream &operator<<(std::ostream &os, const Snapshot &s) {
      os << "{default=" << s.default_ctor << ", value=" << s.value_ctor << ", copy=" << s.copy_ctor
         << ", move=" << s.move_ctor << ", copy_assign=" << s.copy_assign
         << ", move_assign=" << s.move_assign << ", destructor=" << s.destructor << "}";
      return os;
    }
  };

  // Makes the Nth operation that may throw, counted from now, throw TrackedError. Only one throw
  // fires. The destructor disarms it.
  //   Tracked::ThrowAfter guard(3); // the next 3 operations succeed, the 4th throws
  class ThrowAfter {
  public:
    explicit ThrowAfter(std::size_t n) noexcept { s_throw_countdown = n + 1; }
    ~ThrowAfter() { s_throw_countdown = 0; }

    ThrowAfter(const ThrowAfter &) = delete;
    ThrowAfter &operator=(const ThrowAfter &) = delete;
  };

  // value left behind in an object that was moved from
  static constexpr int moved_from = -1;

  Tracked() {
    throw_point();
    born();
    ++s_default_ctor;
  }
  explicit Tracked(const int &value) : m_x(value) {
    throw_point();
    born();
    ++s_value_ctor;
  }

  Tracked(const Tracked &other) : m_x(other.m_x) {
    throw_point();
    other.check_alive("copied from");
    born();
    ++s_copy_ctor;
  }
  Tracked(Tracked &&other) noexcept : m_x(other.m_x) {
    other.check_alive("moved from");
    born();
    other.m_x = moved_from;
    ++s_move_ctor;
  }

  Tracked &operator=(const Tracked &other) {
    throw_point();
    check_alive("copy-assigned to");
    other.check_alive("copied from");
    m_x = other.m_x;
    ++s_copy_assign;
    return *this;
  }

  Tracked &operator=(Tracked &&other) noexcept {
    check_alive("move-assigned to");
    other.check_alive("moved from");
    m_x = other.m_x;
    other.m_x = moved_from;
    ++s_move_assign;
    return *this;
  }

  ~Tracked() {
    // a second destructor call finds nothing to erase
    if (registry().erase(this) == 0) {
      ADD_FAILURE() << "Tracked at " << this << " destroyed twice, or never constructed";
      return;
    }
    ++s_destructor;
  }

  friend bool operator==(const Tracked &lhs, const Tracked &rhs) {
    lhs.check_alive("compared");
    rhs.check_alive("compared");
    return lhs.m_x == rhs.m_x;
  }
  friend std::strong_ordering operator<=>(const Tracked &lhs, const Tracked &rhs) {
    lhs.check_alive("compared");
    rhs.check_alive("compared");
    return lhs.m_x <=> rhs.m_x;
  }

  [[nodiscard]] int value() const {
    check_alive("read");
    return m_x;
  }

  [[nodiscard]] static std::ptrdiff_t live() noexcept {
    return static_cast<std::ptrdiff_t>(s_default_ctor + s_value_ctor + s_copy_ctor + s_move_ctor) -
           static_cast<std::ptrdiff_t>(s_destructor);
  }

  static void reset() noexcept {
    s_default_ctor = s_value_ctor = s_copy_ctor = s_move_ctor = 0;
    s_copy_assign = s_move_assign = s_destructor = 0;
    registry().clear();
    s_throw_countdown = 0;
  }

  [[nodiscard]] static Snapshot snapshot() noexcept {
    return Snapshot(s_default_ctor, s_value_ctor, s_copy_ctor, s_move_ctor, s_copy_assign,
                    s_move_assign, s_destructor);
  }

protected:
  // Every operation that may throw calls this first, before it changes any state. The noexcept
  // moves never call it: a throw out of them would end the program.
  static void throw_point() {
    if (s_throw_countdown == 0) {
      return;
    }
    if (--s_throw_countdown == 0) {
      throw TrackedError();
    }
  }

private:
  // Addresses of the Tracked objects alive right now. The checks look `this` up here and never read
  // the bytes of a dead object, so they don't depend on what a destructor left behind.
  using Registry = std::unordered_set<const Tracked *, std::hash<const Tracked *>, std::equal_to<>,
                                      detail::MallocAllocator<const Tracked *>>;

  // function-local static, so it exists before the first Tracked is built
  static Registry &registry() {
    static Registry s_registry;
    return s_registry;
  }

  // Can throw std::bad_alloc. Inside the noexcept move constructor that ends the test program,
  // which is fine for a test helper.
  void born() const {
    if (!registry().insert(this).second) {
      ADD_FAILURE() << "Tracked constructed at " << this << " over an object that is still alive";
    }
  }

  void check_alive(const char *action) const {
    if (!registry().contains(this)) {
      ADD_FAILURE() << "Tracked at " << this << " " << action << " after it was destroyed";
    }
  }

  static inline std::size_t s_default_ctor = 0;
  static inline std::size_t s_value_ctor = 0;
  static inline std::size_t s_copy_ctor = 0;
  static inline std::size_t s_move_ctor = 0;
  static inline std::size_t s_copy_assign = 0;
  static inline std::size_t s_move_assign = 0;
  static inline std::size_t s_destructor = 0;

  // 0 means disarmed, otherwise the operation that brings it to 0 throws
  static inline std::size_t s_throw_countdown = 0;

  int m_x = 0;
};

static_assert(std::copyable<Tracked>);
static_assert(std::movable<Tracked>);
static_assert(std::is_nothrow_move_constructible_v<Tracked>);
static_assert(std::is_nothrow_move_assignable_v<Tracked>);

// A Tracked whose moves may throw, so they count toward Tracked::ThrowAfter. A vector should copy
// this type when it grows, not move it.
class ThrowingMoveTracked : public Tracked {
public:
  using Tracked::Tracked;

  ThrowingMoveTracked(const ThrowingMoveTracked &) = default;
  // NOLINTNEXTLINE(performance-noexcept-move-constructor): may throw on purpose
  ThrowingMoveTracked(ThrowingMoveTracked &&other) : Tracked(throw_then_move(other)) {}

  ThrowingMoveTracked &operator=(const ThrowingMoveTracked &) = default;
  // NOLINTNEXTLINE(performance-noexcept-move-constructor): may throw on purpose
  ThrowingMoveTracked &operator=(ThrowingMoveTracked &&other) {
    throw_point();
    Tracked::operator=(std::move(other));
    return *this;
  }

  ~ThrowingMoveTracked() = default;

private:
  // runs the throw point before the base move constructor starts
  static Tracked &&throw_then_move(Tracked &other) {
    throw_point();
    return std::move(other);
  }
};

static_assert(!std::is_nothrow_move_constructible_v<ThrowingMoveTracked>);
static_assert(!std::is_nothrow_move_assignable_v<ThrowingMoveTracked>);

// a Tracked that has no default constructor
class NoDefaultTracked : public Tracked {
public:
  explicit NoDefaultTracked(int value) : Tracked(value) {}
};

static_assert(!std::is_default_constructible_v<NoDefaultTracked>);
static_assert(std::copyable<NoDefaultTracked>);

} // namespace stl::test
