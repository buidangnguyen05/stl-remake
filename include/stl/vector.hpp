#pragma once
#include "detail/uninitialized.hpp"
#include "detail/vector_base.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>

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
      transaction.construct_at_end(other.m_start[i]);
    }
    m_finish = transaction.commit();
  }
  vector(vector &&other) noexcept : detail::VectorBase<T>(std::move(other)) {}

  ~vector() { detail::destroy_range(m_start, m_finish); }

  // copy assignment with STRONG guarantee
  vector &strong_copy_assignment(const vector &other) {
    if (this != &other) {
      vector copy(other);
      swap(copy);
    }
    return *this;
  }
  // copy assignment with BASIC guarantee
  vector &basic_copy_assignment(const vector &other) {
    if (this != &other) {
      if (capacity() < other.size()) {
        vector copy(other);
        swap(copy);
      } else if (size() >= other.size()) {
        for (size_type i = 0; i < other.size(); ++i) {
          m_start[i] = other.m_start[i];
        }
        detail::destroy_range(m_start + other.size(), m_finish);
        m_finish = m_start + other.size();
      } else {
        for (size_type i = 0; i < size(); ++i) {
          m_start[i] = other.m_start[i];
        }
        detail::ConstructTransaction<T> transaction(m_start + size());
        for (size_type i = size(); i < other.size(); ++i) {
          transaction.construct_at_end(other.m_start[i]);
        }
        m_finish = transaction.commit();
      }
    }
    return *this;
  }

  vector &operator=(const vector &other) { return basic_copy_assignment(other); }

  vector &operator=(vector &&other) noexcept {
    if (this != &other) {
      detail::destroy_range(m_start, m_finish);
      detail::VectorBase<T>::operator=(std::move(other));
    }
    return *this;
  }

  [[nodiscard]] reference front() { return *m_start; }
  [[nodiscard]] const_reference front() const { return *m_start; }
  [[nodiscard]] reference back() { return *(m_finish - 1); }
  [[nodiscard]] const_reference back() const { return *(m_finish - 1); }
  [[nodiscard]] const_pointer data() const noexcept { return m_start; }
  [[nodiscard]] pointer data() noexcept { return m_start; }
  [[nodiscard]] reference operator[](size_type pos) { return m_start[pos]; }
  [[nodiscard]] const_reference operator[](size_type pos) const { return m_start[pos]; }
  [[nodiscard]] reference at(size_type pos) {
    if (pos >= size()) {
      throw std::out_of_range("vector::at index out of range");
    }
    return m_start[pos];
  }
  [[nodiscard]] const_reference at(size_type pos) const {
    if (pos >= size()) {
      throw std::out_of_range("vector::at index out of range");
    }
    return m_start[pos];
  }

  [[nodiscard]] size_type capacity() const noexcept { return m_end_of_storage - m_start; }
  [[nodiscard]] size_type size() const noexcept { return m_finish - m_start; }
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

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
