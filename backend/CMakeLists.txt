cmake_minimum_required(VERSION 3.14)
project(MemorySimulator VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Shared simulation library (used by both main exe and tests) ───────────────
# Note: main.cpp uses Windows CreateThread directly for portability with MinGW
add_library(cache_lib STATIC
    src/MemoryAddress.cpp
    src/Cache.cpp
    src/ReplacementPolicy.cpp
    src/MemoryHierarchy.cpp
)
target_include_directories(cache_lib PUBLIC include)

# ── Main simulator executable ─────────────────────────────────────────────────
add_executable(memory_sim src/main.cpp)
target_link_libraries(memory_sim PRIVATE cache_lib)

# ── Unit tests ────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
