#include "test_framework.h"

namespace tstest {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

int& failures() {
    static int f = 0;
    return f;
}

} // namespace tstest

int main() {
    int total = 0;
    for (auto& t : tstest::registry()) {
        const int before = tstest::failures();
        t.fn();
        const int after = tstest::failures();
        std::printf("[ %s ] %s\n", after == before ? "PASS" : "FAIL", t.name.c_str());
        ++total;
    }
    std::printf("\n%d tests, %d failure(s)\n", total, tstest::failures());
    return tstest::failures() == 0 ? 0 : 1;
}
