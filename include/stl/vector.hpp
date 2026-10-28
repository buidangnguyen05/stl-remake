#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>

namespace stl {

template <typename T> class vector {
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

  vector() noexcept = default;

  ~vector() {
    for (pointer p = m_start; p != m_finish; ++p) {
      p->~T();
    }
    deallocate(m_start, capacity());
  }

  [[nodiscard]] size_type max_size() const noexcept {
    return std::min<size_type>(std::numeric_limits<size_type>::max(),
                               std::numeric_limits<difference_type>::max()) /
           sizeof(T);
  }

  [[nodiscard]] size_type capacity() const noexcept { return m_end_of_storage - m_start; }
  [[nodiscard]] size_type size() const noexcept { return m_finish - m_start; }
  [[nodiscard]] const_pointer data() const noexcept { return m_start; }
  [[nodiscard]] pointer data() noexcept { return m_start; }

private:
  pointer allocate(size_type n) {
    if (n == 0) {
      return nullptr;
    }
    pointer p = static_cast<pointer>(
        ::operator new(n * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
    return p;
  }
  void deallocate(pointer p, size_type n) {
    if (p) {
      ::operator delete(p, n * sizeof(T), static_cast<std::align_val_t>(alignof(T)));
    }
  }

  pointer m_start = nullptr;
  pointer m_finish = nullptr;
  pointer m_end_of_storage = nullptr;
};

} // namespace stl
