project(tinyjson)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(TEST_SRCS "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
set(LIB_SRCS "${CMAKE_CURRENT_LIST_DIR}/tinyjson.cpp")

add_executable(tinytest "${TEST_SRCS}")
target_link_libraries(tinytest tinyjson)

add_library(tinyjson STATIC "${LIB_SRCS}")
