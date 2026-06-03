#pragma once

#include <cassert>

// Assertions intended for real-time code paths. Compiled out in release builds so
// they never cost anything in process(); active in debug to catch contract breaks
// (e.g. a node used before prepare(), or a block larger than maxBlockSize).
#ifndef NDEBUG
  #define TS_RT_ASSERT(cond) assert(cond)
#else
  #define TS_RT_ASSERT(cond) ((void)0)
#endif
