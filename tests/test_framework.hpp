#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

// 简易测试框架
namespace test
{

    struct TestCase
    {
        std::string name;
        std::function<void()> func;
    };

    inline std::vector<TestCase> &get_tests()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline int &fail_count()
    {
        static int count = 0;
        return count;
    }

    inline int &pass_count()
    {
        static int count = 0;
        return count;
    }

    struct TestRegistrar
    {
        TestRegistrar(const char *name, std::function<void()> func)
        {
            get_tests().push_back({name, std::move(func)});
        }
    };

    inline void check_impl(bool condition, const char *expr, const char *file, int line)
    {
        if (!condition)
        {
            std::cerr << "  FAILED: " << expr << " at " << file << ":" << line << "\n";
            fail_count()++;
        }
        else
        {
            pass_count()++;
        }
    }

    inline void require_impl(bool condition, const char *expr, const char *file, int line)
    {
        if (!condition)
        {
            std::cerr << "  FAILED: " << expr << " at " << file << ":" << line << "\n";
            fail_count()++;
            throw std::runtime_error("REQUIRE failed");
        }
        else
        {
            pass_count()++;
        }
    }

    inline int run_all()
    {
        int failed_tests = 0;
        for (const auto &test : get_tests())
        {
            std::cout << "Running: " << test.name << "\n";
            try
            {
                test.func();
            }
            catch (const std::exception &e)
            {
                std::cerr << "  Exception: " << e.what() << "\n";
                failed_tests++;
                continue;
            }
            std::cout << "  PASSED\n";
        }

        std::cout << "\n========================================\n";
        std::cout << "Total: " << get_tests().size() << " tests\n";
        std::cout << "Checks passed: " << pass_count() << "\n";
        std::cout << "Checks failed: " << fail_count() << "\n";
        std::cout << "Tests failed: " << failed_tests << "\n";

        return failed_tests > 0 || fail_count() > 0 ? 1 : 0;
    }

} // namespace test

#define TEST_CASE(name)                                                   \
    static void test_func_##name();                                       \
    static test::TestRegistrar registrar_##name(#name, test_func_##name); \
    static void test_func_##name()

#define CHECK(expr) test::check_impl((expr), #expr, __FILE__, __LINE__)
#define REQUIRE(expr) test::require_impl((expr), #expr, __FILE__, __LINE__)

#define CHECK_THAT(str, matcher) \
    test::check_impl((matcher).matches(str), #str " matches " #matcher, __FILE__, __LINE__)

// 简单的字符串匹配器
struct ContainsSubstring
{
    std::string substr;
    ContainsSubstring(const std::string &s) : substr(s) {}
    bool matches(const std::string &str) const
    {
        return str.find(substr) != std::string::npos;
    }
};
