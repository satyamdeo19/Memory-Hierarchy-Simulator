cmake_minimum_required(VERSION 3.10)
project(MemorySimulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(memory_sim 
    src/main.cpp
    src/MemoryAddress.cpp
    src/Cache.cpp
    src/ReplacementPolicy.cpp
    src/MemoryHierarchy.cpp
)
