#pragma once
#include "detail/uninitialized.hpp"
#include "detail/vector_base.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>

namespace stl {
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
  vector(const vector &other) : detail::VectorBase<T>(other.size()) {
    detail::ConstructTransaction<T> transaction(m_start);
    for (size_type i = 0; i < other.size(); ++i) {
      transaction.construct_at_end(*(other.m_start + i));
    }
    m_finish = transaction.commit();
  }
  vector(vector &&other) noexcept : detail::VectorBase<T>(std::move(other)) {}

  ~vector() { detail::destroy_range(m_start, m_finish); }

  [[nodiscard]] size_type capacity() const noexcept { return m_end_of_storage - m_start; }
  [[nodiscard]] size_type size() const noexcept { return m_finish - m_start; }
  [[nodiscard]] const_pointer data() const noexcept { return m_start; }
  [[nodiscard]] pointer data() noexcept { return m_start; }

  void swap(vector &other) noexcept { detail::VectorBase<T>::swap(other); }

private:
  using detail::VectorBase<T>::m_start;
  using detail::VectorBase<T>::m_finish;
  using detail::VectorBase<T>::m_end_of_storage;
};

template <typename T> void swap(vector<T> &a, vector<T> &b) noexcept {
  a.swap(b);
}

} // namespace stl
