// SPDX-License-Identifier: GPL-3.0-or-later
//
// A deliberately tiny test harness, so the core can be tested without
// third-party dependencies.
//
//   NGS_TEST(name) { NGS_CHECK(1 + 1 == 2); }
#pragma once

#include <functional>
#include <string_view>
#include <vector>

namespace nagus::test {

struct Case {
    std::string_view name;
    std::function<void()> body;
};

std::vector<Case>& registry();
void reportFailure(const char* file, int line, const char* expression);
void reportSkip(const char* reason);

struct Registrar {
    Registrar(std::string_view name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

} // namespace nagus::test

#define NGS_TEST_CONCAT_INNER(a, b) a##b
#define NGS_TEST_CONCAT(a, b) NGS_TEST_CONCAT_INNER(a, b)

#define NGS_TEST(name)                                                                     \
    static void NGS_TEST_CONCAT(ngs_test_fn_, __LINE__)();                                 \
    static const ::nagus::test::Registrar NGS_TEST_CONCAT(ngs_test_reg_, __LINE__){          \
        name, &NGS_TEST_CONCAT(ngs_test_fn_, __LINE__)};                                   \
    static void NGS_TEST_CONCAT(ngs_test_fn_, __LINE__)()

#define NGS_CHECK(...)                                                              \
    do {                                                                                   \
        if (!(__VA_ARGS__)) {                                                               \
            ::nagus::test::reportFailure(__FILE__, __LINE__, #__VA_ARGS__);                   \
        }                                                                                  \
    } while (false)

// Ends the current test early and reports it as skipped, for tests that need
// files which are not always present (such as the original game data).
#define NGS_SKIP(reason)                                                                   \
    do {                                                                                   \
        ::nagus::test::reportSkip(reason);                                                   \
        return;                                                                            \
    } while (false)
