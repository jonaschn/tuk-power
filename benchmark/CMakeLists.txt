cmake_minimum_required(VERSION 3.10)
project(benchmark)

set(CMAKE_CXX_STANDARD 14)

add_compile_options(-O3)

add_executable(benchmark main.cpp)
add_executable(prefetching_intel prefetching_intel.cpp)

find_package (Threads)
target_link_libraries (benchmark      ${CMAKE_THREAD_LIBS_INIT})

# target_include_directories(benchmark PRIVATE /opt/ibm/xlC/lib)
