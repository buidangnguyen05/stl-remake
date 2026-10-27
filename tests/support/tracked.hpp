#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <ostream>
#include <type_traits>

namespace stl::test {

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

  Tracked() { ++s_default_ctor; }
  explicit Tracked(int value) : m_x(value) { ++s_value_ctor; }

  Tracked(const Tracked &other) : m_x(other.m_x) { ++s_copy_ctor; }
  Tracked(Tracked &&other) noexcept : m_x(other.m_x) { ++s_move_ctor; }

  Tracked &operator=(const Tracked &other) {
    m_x = other.m_x;
    ++s_copy_assign;
    return *this;
  }

  Tracked &operator=(Tracked &&other) noexcept {
    m_x = other.m_x;
    ++s_move_assign;
    return *this;
  }

  ~Tracked() { ++s_destructor; }

  auto operator<=>(const Tracked &) const = default;

  [[nodiscard]] int value() const noexcept { return m_x; }

  [[nodiscard]] static std::ptrdiff_t live() noexcept {
    return static_cast<std::ptrdiff_t>(s_default_ctor + s_value_ctor + s_copy_ctor + s_move_ctor) -
           static_cast<std::ptrdiff_t>(s_destructor);
  }

  static void reset() noexcept {
    s_default_ctor = s_value_ctor = s_copy_ctor = s_move_ctor = 0;
    s_copy_assign = s_move_assign = s_destructor = 0;
  }

  [[nodiscard]] static Snapshot snapshot() noexcept {
    return {s_default_ctor, s_value_ctor,  s_copy_ctor, s_move_ctor,
            s_copy_assign,  s_move_assign, s_destructor};
  }

private:
  static inline std::size_t s_default_ctor = 0;
  static inline std::size_t s_value_ctor = 0;
  static inline std::size_t s_copy_ctor = 0;
  static inline std::size_t s_move_ctor = 0;
  static inline std::size_t s_copy_assign = 0;
  static inline std::size_t s_move_assign = 0;
  static inline std::size_t s_destructor = 0;

  int m_x = 0;
};

static_assert(std::copyable<Tracked>);
static_assert(std::movable<Tracked>);
static_assert(std::is_nothrow_move_constructible_v<Tracked>);
static_assert(std::is_nothrow_move_assignable_v<Tracked>);

} // namespace stl::test
