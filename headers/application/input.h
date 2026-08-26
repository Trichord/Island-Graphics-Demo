#pragma once

#include <cstdint>

struct Input
{
    enum Direction : std::uint8_t
    {
        none = 0,
        forward = 1U,
        backward = 2U,
        left = 4U,
        right = 8U
    };

    bool sprintToggle{false};
    bool crouchToggle{false};
    bool jump{false};

    Direction direction{Direction::none};

    float mouseDeltaX{0.0f};
    float mouseDeltaY{0.0f};
};

constexpr Input::Direction operator|(Input::Direction lhs, Input::Direction rhs) noexcept
{
    return static_cast<Input::Direction>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr Input::Direction &operator|=(Input::Direction &lhs, Input::Direction rhs) noexcept
{
    return lhs = lhs | rhs;
}