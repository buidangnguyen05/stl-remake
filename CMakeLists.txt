cmake_minimum_required(VERSION 3.20...4.2.3)

project(stl-remake CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

add_library(stl INTERFACE)
add_library(stl::stl ALIAS stl)

target_include_directories(stl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(stl INTERFACE cxx_std_23)

function(set_warning_flags TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Warning flags cannot be set on non-target ${TARGET}")
    endif()
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endfunction()

add_subdirectory(tests)
