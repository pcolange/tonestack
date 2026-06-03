#pragma once

// Minimal, dependency-free test framework for Phase 0. Tests self-register via TS_TEST;
// test_main.cpp runs them and returns non-zero on any failure. Replaced/augmented by
// Catch2 once vendored dependencies are wired in.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace tstest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();
int& failures();

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline void checkTrue(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        ++failures();
        std::printf("  FAIL %s:%d  CHECK(%s)\n", file, line, expr);
    }
}

inline void checkNear(double a, double b, double tol, const char* ea, const char* eb,
                      const char* file, int line) {
    if (std::fabs(a - b) > tol) {
        ++failures();
        std::printf("  FAIL %s:%d  CHECK_NEAR(%s, %s)  %.9g vs %.9g\n",
                    file, line, ea, eb, a, b);
    }
}

} // namespace tstest

#define TS_TEST(name)                                                   \
    static void name();                                                 \
    static ::tstest::Registrar ts_reg_##name(#name, name);             \
    static void name()

#define TS_CHECK(cond) ::tstest::checkTrue((cond), #cond, __FILE__, __LINE__)

#define TS_CHECK_NEAR(a, b, tol)                                                       \
    ::tstest::checkNear(static_cast<double>(a), static_cast<double>(b),                \
                        static_cast<double>(tol), #a, #b, __FILE__, __LINE__)
