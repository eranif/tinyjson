cmake_minimum_required(VERSION 3.2)
set(ENV{CC} clang)
set(ENV{CXX} clang++)

project(tinyjson)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # build with ASAN for our all of our targets
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -O0")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -O0")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(TEST_SRCS "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
set(LIB_SRCS "${CMAKE_CURRENT_LIST_DIR}/tinyjson.cpp")
add_executable(tinytest "${TEST_SRCS}")
target_link_libraries(tinytest tinyjson)

add_library(tinyjson STATIC "${LIB_SRCS}")
