cmake_minimum_required(VERSION 3.14)

# ── Test executable using doctest (single-header, zero external deps) ─────────
# doctest.h is vendored in the tests/ directory — no FetchContent required.
add_executable(run_tests
    test_cache.cpp
    test_replacement.cpp
    test_memory_hierarchy.cpp
)

target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # picks up doctest.h
)

target_link_libraries(run_tests PRIVATE cache_lib)

# Register with ctest
include(CTest)
add_test(NAME MemorySimulatorTests COMMAND run_tests)
