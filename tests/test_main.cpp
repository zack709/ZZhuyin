#include "test_runner.h"

#include <iostream>

int main() {
    int passed = 0;
    int failed = 0;

    for (const auto& tc : zzhuyin::test::Registry()) {
        zzhuyin::test::CurrentTestFailures() = 0;
        tc.fn();
        if (zzhuyin::test::CurrentTestFailures() == 0) {
            std::cout << "[PASS] " << tc.name << "\n";
            ++passed;
        } else {
            std::cout << "[FAIL] " << tc.name << "\n";
            ++failed;
        }
    }

    std::cout << "\nSummary: " << passed << " passed, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
