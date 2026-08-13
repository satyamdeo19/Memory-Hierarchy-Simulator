// crtdbg.h stub for MinGW — suppresses the missing-header error in gtest.cc
// MinGW doesn't ship crtdbg.h (a MSVC runtime debugging header).
// GTest only uses it when GTEST_HAS_SEH != 0, but the include is unconditional
// in gtest.cc on Windows, so we provide an empty stub.
#pragma once
// intentionally empty — MinGW doesn't need MSVC's CRT debug facilities
