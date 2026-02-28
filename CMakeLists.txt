cmake_minimum_required(VERSION 3.10.0)
project(VoxEcs VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 



add_library(VoxEcs INTERFACE)
target_include_directories(VoxEcs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})


