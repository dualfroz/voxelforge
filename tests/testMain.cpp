#include "testHarness.h"

#include <cstdio>

namespace vftest {

int failures = 0;
int checks = 0;

std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

int registerTest(const char* name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
    return 0;
}

void reportCheck(bool ok, const char* expr, const char* file, int line) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s:%d  %s\n", file, line, expr);
    }
}

}

int main() {
    int failedTests = 0;
    for (auto& test : vftest::registry()) {
        int before = vftest::failures;
        test.fn();
        bool passed = vftest::failures == before;
        std::printf("[%s] %s\n", passed ? "PASS" : "FAIL", test.name.c_str());
        if (!passed) ++failedTests;
    }
    std::printf("\n%zu tests, %d checks, %d failed checks, %d failed tests\n",
                vftest::registry().size(), vftest::checks, vftest::failures, failedTests);
    return vftest::failures == 0 ? 0 : 1;
}
