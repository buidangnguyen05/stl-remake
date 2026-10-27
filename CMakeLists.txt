cmake_minimum_required(VERSION 4.2)

project(stl-remake CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

option(STL_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})

add_library(stl INTERFACE)
add_library(stl::stl ALIAS stl)

target_include_directories(stl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(stl INTERFACE cxx_std_23)

### gtest
if (STL_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            SYSTEM
            URL https://github.com/google/googletest/releases/download/v1.18.0/googletest-1.18.0.tar.gz
            URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    )
    FetchContent_MakeAvailable(googletest)
endif ()
###

function(set_warning_flags TARGET)
    if (NOT TARGET ${TARGET})
        message(FATAL_ERROR "Warning flags cannot be set on non-target ${TARGET}")
    endif ()
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endfunction()

set(CACHE{STL_SANITIZE} TYPE STRING HELP "Set sanitization flags" VALUE "")
if (STL_SANITIZE)
    add_compile_options(-fsanitize=${STL_SANITIZE} -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${STL_SANITIZE})
endif ()

add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)

if (STL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
