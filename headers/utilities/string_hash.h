#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

// Credit: https://www.cppstories.com/2021/heterogeneous-access-cpp20/
struct StringHash
{
    using is_transparent = void;

    [[nodiscard]] size_t operator()(const char *txt) const noexcept
    {
        return std::hash<std::string_view>{}(txt);
    }

    [[nodiscard]] size_t operator()(std::string_view txt) const noexcept
    {
        return std::hash<std::string_view>{}(txt);
    }

    [[nodiscard]] size_t operator()(const std::string &txt) const noexcept
    {
        return std::hash<std::string_view>{}(txt);
    }
};
