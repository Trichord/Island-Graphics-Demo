#pragma once

#include <cstdint>

struct UserInput
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

constexpr UserInput::Direction operator|(UserInput::Direction lhs, UserInput::Direction rhs) noexcept
{
    return static_cast<UserInput::Direction>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr UserInput::Direction &operator|=(UserInput::Direction &lhs, UserInput::Direction rhs) noexcept
{
    return lhs = lhs | rhs;
}