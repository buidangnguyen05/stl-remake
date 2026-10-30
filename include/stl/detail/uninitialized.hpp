#pragma once
#include <cstddef>
#include <memory>
#include <utility>

namespace stl::detail {
template <typename T> void destroy_range(T *start, T *finish) noexcept {
  for (; start != finish; ++start) {
    std::destroy_at(start);
  }
}

template <typename T> class ConstructTransaction {
public:
  explicit ConstructTransaction(T *p) : m_p(p) {}
  ConstructTransaction(const ConstructTransaction &) = delete;
  ConstructTransaction &operator=(const ConstructTransaction &) = delete;

  ~ConstructTransaction() { destroy_range(m_p, m_p + m_built); }

  template <typename... Args> void construct_at_end(Args &&...args) {
    std::construct_at(m_p + m_built, std::forward<Args>(args)...);
    ++m_built;
  }

  [[nodiscard]] T *commit() { return m_p += std::exchange(m_built, 0); }

private:
  T *m_p = nullptr;
  std::size_t m_built = 0;
};
} // namespace stl::detail
