cmake_minimum_required(VERSION 3.16)
project(KVAllocator)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -O2
)

add_executable(kv_allocator
    src/main.cpp
    src/arena_allocator.cpp
)

target_include_directories(kv_allocator PRIVATE include)