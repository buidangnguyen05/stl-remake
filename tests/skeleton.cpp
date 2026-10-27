#include <iostream>
#include <stl/version.hpp>
#include <support/tracked.hpp>
#include <vector>

signed main() {
  std::cout << stl::version << "\n";

  stl::test::Tracked(5);

  std::vector<stl::test::Tracked> v(5, stl::test::Tracked(10));

  std::cout << "Live objects: " << stl::test::Tracked::live() << "\n";
}
