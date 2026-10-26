#pragma once

#include <cctype>
#include <string>
#include <string_view>

// 内部工具函数,不对外暴露

namespace http_utils
{

    // 大小写不敏感字符串比较(RFC 7230 头部字段名比较)
    // std::string / const char* 均可隐式转换为 std::string_view
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

} // namespace http_utils
