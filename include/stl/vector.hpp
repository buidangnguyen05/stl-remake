#pragma once
#include "detail/uninitialized.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <new>
#include <stdexcept>

namespace stl {
namespace detail {
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
} // namespace detail

template <typename T> class vector : detail::VectorBase<T> {
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T &;
  using const_reference = const T &;
  using pointer = T *;
  using const_pointer = const T *;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using detail::VectorBase<T>::max_size;

  vector() noexcept = default;
  explicit vector(size_type n) : detail::VectorBase<T>(n) {
    detail::ConstructTransaction<T> transaction(m_start);
    for (size_type i = 0; i < n; ++i) {
      transaction.construct_at_end();
    }
    m_finish = transaction.commit();
  }
  vector(size_type n, const T &value) : detail::VectorBase<T>(n) {
    detail::ConstructTransaction<T> transaction(m_start);
    for (size_type i = 0; i < n; ++i) {
      transaction.construct_at_end(value);
    }
    m_finish = transaction.commit();
  }
  vector(std::initializer_list<T> ilist) : detail::VectorBase<T>(ilist.size()) {
    detail::ConstructTransaction<T> transaction(m_start);
    for (const auto &value : ilist) {
      transaction.construct_at_end(value);
    }
    m_finish = transaction.commit();
  }

  ~vector() { detail::destroy_range(m_start, m_finish); }

  [[nodiscard]] size_type capacity() const noexcept { return m_end_of_storage - m_start; }
  [[nodiscard]] size_type size() const noexcept { return m_finish - m_start; }
  [[nodiscard]] const_pointer data() const noexcept { return m_start; }
  [[nodiscard]] pointer data() noexcept { return m_start; }

private:
  using detail::VectorBase<T>::m_start;
  using detail::VectorBase<T>::m_finish;
  using detail::VectorBase<T>::m_end_of_storage;
};

} // namespace stl
