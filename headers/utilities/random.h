#pragma once

#include <cstddef>

namespace random
{
// Returns a random index between [0, size)
std::size_t index(std::size_t size);

// Returns a random float in the [min, max) range using a minimal standard generator
float floatMS(float min, float max);

// Returns a random float in the [min, max) range using a Mersenne Twister generator
float floatMT(float min, float max);
} // namespace random