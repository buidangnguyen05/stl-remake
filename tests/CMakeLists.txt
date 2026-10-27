add_library(stl_test_support INTERFACE)
target_include_directories(stl_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(skeleton skeleton.cpp)
target_link_libraries(skeleton PRIVATE
        stl::stl
        stl_test_support
)
set_warning_flags(skeleton)

include(GoogleTest)

add_executable(gtest_smoke gtest_smoke.cpp)
target_link_libraries(gtest_smoke PRIVATE
        stl::stl
        GTest::gtest_main
        stl_test_support
)
set_warning_flags(gtest_smoke)
gtest_discover_tests(gtest_smoke)

add_executable(test_tracked test_tracked.cpp)
target_link_libraries(test_tracked PRIVATE
        stl::stl
        stl_test_support
        GTest::gtest_main
)
set_warning_flags(test_tracked)
gtest_discover_tests(test_tracked)