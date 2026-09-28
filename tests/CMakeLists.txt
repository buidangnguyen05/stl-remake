add_executable(skeleton skeleton.cpp)
target_link_libraries(skeleton PRIVATE stl::stl)
set_warning_flags(skeleton)
