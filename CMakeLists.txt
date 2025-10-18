cmake_minimum_required(VERSION 3.10.0)
project(VoxEcs VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 



add_executable(VoxEcs thread_pool.h dynamic_bitset.h vox_ecs.h main.cpp)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific flags
if(MSVC)
    # /O2 = maximize speed, /Ot = favor fast code
    target_compile_options(VoxEcs PRIVATE /O2 /Ot)
else()
    # GCC/Clang flags for optimization
    target_compile_options(VoxEcs PRIVATE -O3 -march=native -flto)
endif()