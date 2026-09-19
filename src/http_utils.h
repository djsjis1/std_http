#pragma once

#include <cctype>
#include <string>
#include <string_view>

// 内部工具函数,不对外暴露

namespace http_utils
{

    // 大小写不敏感字符串比较(RFC 7230 头部字段名比较)
    inline bool iequals(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
    }

    // 大小写不敏感比较(std::string 与 const char*)
    inline bool iequals(const std::string &a, const char *b)
    {
        size_t n = 0;
        while (b[n])
            ++n;
        if (a.size() != n)
            return false;
        for (size_t i = 0; i < n; ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
    }

} // namespace http_utils
