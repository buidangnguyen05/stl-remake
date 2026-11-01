#pragma once
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stl::detail {
template <typename T> struct VectorBase {
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;

  VectorBase() noexcept = default;
  explicit VectorBase(size_type n)
      : m_start(allocate(n)), m_finish(m_start), m_end_of_storage(m_start + n) {}
  ~VectorBase() noexcept { deallocate(m_start, m_end_of_storage - m_start); }

  VectorBase(const VectorBase &other) = delete;
  VectorBase &operator=(const VectorBase &other) = delete;

  VectorBase(VectorBase &&other) noexcept
      : m_start(std::exchange(other.m_start, nullptr)),
        m_finish(std::exchange(other.m_finish, nullptr)),
        m_end_of_storage(std::exchange(other.m_end_of_storage, nullptr)) {}
  VectorBase &operator=(VectorBase &&other) noexcept {
    if (this != &other) {
      deallocate(m_start, m_end_of_storage - m_start);

      m_start = std::exchange(other.m_start, nullptr);
      m_finish = std::exchange(other.m_finish, nullptr);
      m_end_of_storage = std::exchange(other.m_end_of_storage, nullptr);
    }
    return *this;
  }

  void swap(VectorBase &other) noexcept {
    using std::swap;
    swap(m_start, other.m_start);
    swap(m_finish, other.m_finish);
    swap(m_end_of_storage, other.m_end_of_storage);
  }

  [[nodiscard]] size_type max_size() const noexcept {
    return std::min<size_type>(std::numeric_limits<size_type>::max(),
                               std::numeric_limits<difference_type>::max()) /
           sizeof(T);
  }

  pointer allocate(size_type n) {
    if (n == 0) {
      return nullptr;
    }
    if (n > max_size()) {
      throw std::length_error("cannot create vector larger than max_size()");
    }
    auto p = static_cast<pointer>(
        ::operator new(n * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
    return p;
  }
  void deallocate(pointer p, size_type n) noexcept {
    if (p) {
      ::operator delete(p, n * sizeof(T), static_cast<std::align_val_t>(alignof(T)));
    }
  }

  pointer m_start = nullptr;
  pointer m_finish = nullptr;
  pointer m_end_of_storage = nullptr;
};
} // namespace stl::detail
