add_executable(skeleton skeleton.cpp)
target_link_libraries(skeleton PRIVATE stl::stl)
set_warning_flags(skeleton)

include(GoogleTest)

add_executable(gtest_smoke gtest_smoke.cpp)
target_link_libraries(gtest_smoke PRIVATE gtest_main)
set_warning_flags(gtest_smoke)

gtest_discover_tests(gtest_smoke)