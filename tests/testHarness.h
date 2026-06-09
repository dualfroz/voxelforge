#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace vftest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();
int registerTest(const char* name, std::function<void()> fn);
void reportCheck(bool ok, const char* expr, const char* file, int line);

extern int failures;
extern int checks;

} // namespace vftest

#define VF_TEST(name)                                                     \
    static void name();                                                   \
    static int name##Registration = vftest::registerTest(#name, name);    \
    static void name()

#define CHECK(cond) vftest::reportCheck((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) vftest::reportCheck((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) \
    vftest::reportCheck(std::fabs((a) - (b)) <= (eps), #a " ~= " #b, __FILE__, __LINE__)
