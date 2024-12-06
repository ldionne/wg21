cmake_minimum_required(VERSION 3.9)
project(p3516_benchmarks)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Wall -Wextra)

include(CTest)

add_executable(vector_bench main.cpp)
add_test(NAME vector_bench COMMAND vector_bench)
