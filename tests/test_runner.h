#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace zzhuyin::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& CurrentTestFailures() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        Registry().push_back(TestCase{name, std::move(fn)});
    }
};

inline std::string U16ToHex(const std::u16string& s) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < s.size(); ++i) {
        if (i > 0) oss << " ";
        oss << "U+" << std::hex << std::uppercase << static_cast<uint32_t>(s[i]);
    }
    oss << "]";
    return oss.str();
}

} // namespace zzhuyin::test

#define TEST(Name)                                                                 \
    static void TestFn_##Name();                                                   \
    static ::zzhuyin::test::Registrar kReg_##Name(#Name, TestFn_##Name);           \
    static void TestFn_##Name()

#define EXPECT_TRUE(cond)                                                          \
    do {                                                                           \
        if (!(cond)) {                                                             \
            ++::zzhuyin::test::CurrentTestFailures();                              \
            std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] "         \
                      << "EXPECT_TRUE(" #cond ")\n";                               \
        }                                                                          \
    } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

#define EXPECT_EQ(a, b)                                                            \
    do {                                                                           \
        const auto& _va = (a);                                                     \
        const auto& _vb = (b);                                                     \
        if (!(_va == _vb)) {                                                       \
            ++::zzhuyin::test::CurrentTestFailures();                              \
            std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] "         \
                      << "EXPECT_EQ(" #a ", " #b ")\n";                            \
        }                                                                          \
    } while (0)
