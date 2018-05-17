cmake_minimum_required(VERSION 3.10)
project(benchmark)

set(CMAKE_CXX_STANDARD 14)

add_compile_options(-O3)

add_executable(benchmark main.cpp)